#include "hook.h"
#include "asm.h"
#include "data.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;

static std::optional<std::string> calculateSHA256();
static void initPatchHook();
std::optional<std::vector<BYTE>> readBinFile(const std::string& path);
std::optional<std::string> readTextFile(const std::string& path);
static void updateMemoryData(void* address, std::vector<BYTE> bytes);
static std::vector<BYTE> assembleCallOp(void* targetFuncAddr, void* hookFuncAddr);
static void writeHook(void* targetFuncAddr, void* hookFuncAddr);
static void writeHookWithNop(void* targetFuncAddr, void* hookFuncAddr, unsigned nopLength);
static void skipAsmCode(void* targetAddr, size_t codeLength);
static std::string transferToGbk(const std::string& str);

void HookInit(HMODULE hDll) {
	std::optional<std::string> sha256 = calculateSHA256();
	if (sha256 && *sha256 != PROCESS_FILE_SHA256) {
		int btn = MessageBoxA(NULL, SHA256_MISMATCH_WARNING, MESSAGEBOX_TITLE, MB_YESNO | MB_ICONWARNING);
		if (btn != IDYES) {
			return;
		}
	}
	MessageBoxA(NULL, ALPHA_VERSION_WARNING, MESSAGEBOX_TITLE, MB_ICONWARNING);
	PatchPack pack{};
	std::optional<std::vector<BYTE>> binFile = readBinFile(BIN_DATA_FILE);
	std::optional<PatchPack> textFile = PackFromTextFile();
	if (binFile) {
		std::vector<BYTE> res = std::move(*binFile);
		TransferBinBytes(res);
		pack.loadFromBinFile(res.cbegin(), res.cend());
		if (textFile) {
			pack.update(std::move(*textFile));
		}
		patch_rt = std::make_unique<PatchRT>(std::move(pack));
	} else if (textFile) {
		patch_rt = std::make_unique<PatchRT>(std::move(*textFile));
	} else {
		return;
	}
	initPatchHook();
}

void PatchHookAfterVMInit() {
	image_base = static_cast<void*>(GetModuleHandleA(PROCESS_NAME));
	auto addr = [](DWORD rva) {
			return reinterpret_cast<void*>(reinterpret_cast<DWORD>(image_base) + rva); 
		};
	auto change = [](void* addr) {
			std::vector<BYTE> v;
			v.reserve(4);
			DWORD d = reinterpret_cast<DWORD>(addr);
			v.push_back(static_cast<BYTE>(d & 0xFF));
			v.push_back(static_cast<BYTE>((d >> 8) & 0xFF));
			v.push_back(static_cast<BYTE>((d >> 16) & 0xFF));
			v.push_back(static_cast<BYTE>((d >> 24) & 0xFF));
			return v;
		};

	patch_rt->setTranslatedPERes(image_base);
	context_pointer = static_cast<RealLiveVMContext*>(addr(RLVM_CONTEXT_POINTET_RVA));
	state_pointer = static_cast<RealLiveVMState*>(addr(RLVM_STATE_POINTER_RVA));
	
	for (size_t i = 0; i < SET_NOP_ARRAY_SIZE; ++i) {
		skipAsmCode(addr(SET_NOP_RVA[i]), SET_NOP_COUNT[i]);
	}
	for (size_t i = 0; i < SET_OPCODE_ARRAY_SIZE; ++i) {
		updateMemoryData(addr(SET_OPCODE_RVA[i]), SET_OPCODE[i]);
	}

	std::vector<std::pair<DWORD, void*>> prepareHook = {
		{OPEN_G00_FILE_FUNC_CALLER_RVA, ProxyOpenG00File},
		{HOOK_SELECT_TEXT_RVA, HookSelectText},
		{HOOK_CHUNK_TEXT_LEN_RVA, HookChunkTextLength},
		{HOOK_CHUNK_TEXT_RVA, HookChunkText},
		{HANDLE_NAME_TEXT_CALLER_RVA, HookNameText}
	};
	for (size_t i = 0; i < CONSUME_TEXT_IN_QUOTE_MODE_CALLER_SIZE; ++i) {
		prepareHook.push_back({ CONSUME_TEXT_IN_QUOTE_MODE_CALLER_RVA[i], ProxyConsumeTextInQuoteMode });
	}
	for (const auto& [t, h] : prepareHook) {
		writeHook(addr(t), h);
	}

	std::vector<std::pair<DWORD, std::vector<BYTE>>> prepareUpdate = {
		{INVALIDATE_RECT_IAT_RVA, change(ProxyInvalidateRect)},
		{SET_DIB_TO_DEVICE_IAT_RVA, change(ProxySetDIBitsToDevice)},
		{GAME_QUIT_MESSAGE_ADDR_RVA, change(const_cast<char*>(GAME_QUIT_MESSAGE))}
	};
	std::vector<BYTE> patch = assembleCallOp(addr(READ_SEEN_DATA_FUNC_RVA), HookForPatch);
	patch.push_back(static_cast<BYTE>(0xC3));
	prepareUpdate.push_back({ READ_SEEN_DATA_FUNC_RVA, patch });
	std::vector<BYTE> patch2 = assembleCallOp(addr(RLD3D_PRESENT_CALLER_RVA), ProxyRld3dPresent);
	patch2.push_back(static_cast<BYTE>(0x90));
	prepareUpdate.push_back({ RLD3D_PRESENT_CALLER_RVA, patch2 });
	for (size_t i = 0; i < WINDOW_TITLE_ADDR_RVA_SIZE; ++i) {
		prepareUpdate.push_back({ WINDOW_TITLE_ADDR_RVA[i], change(const_cast<char*>(WINDOW_TITLE_TEXT))});
	}
	for (const auto& [t, v] : prepareUpdate) {
		updateMemoryData(addr(t), v);
	}
	updateMemoryData(CreateWindowExA, WINAPI_PATCH_STUB);	
}

void HookDestroy() {
	patch_rt.reset();
}

static std::optional<std::string> calculateSHA256() {
	std::optional<std::vector<BYTE>> data = readBinFile(PROCESS_NAME);
	if (!data) {
		return std::nullopt;
	}

	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	std::array<BYTE, 32> out_hash{};
	bool ok = false;

	if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
		if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
			ok = true;
			if (!data->empty()) {
				if (data->size() > (std::numeric_limits<DWORD>::max)()) {
					ok = false;
				} else if (!CryptHashData(hHash, data->data(), static_cast<DWORD>(data->size()), 0)) {
					ok = false;
				}
			}
			if (ok) {
				DWORD hashLen = static_cast<DWORD>(out_hash.size());
				ok = CryptGetHashParam(hHash, HP_HASHVAL, out_hash.data(), &hashLen, 0) != FALSE;
			}
			CryptDestroyHash(hHash);
		}
		CryptReleaseContext(hProv, 0);
	}

	if (!ok) {
		return std::nullopt;
	}

	static constexpr char hexDigits[] = "0123456789abcdef";
	std::string result(64, '\0');
	for (size_t i = 0; i < out_hash.size(); ++i) {
		result[i * 2] = hexDigits[(out_hash[i] >> 4) & 0xF];
		result[i * 2 + 1] = hexDigits[out_hash[i] & 0xF];
	}
	return result;
}

static void initPatchHook() {
	std::vector<std::pair<void*, void*>> addr = {
		{GetProcAddress(GetModuleHandleA("kernelbase.dll"), "CreateFileA"), HookCreateFileA},
		{CreateWindowExA, HookCreateWindowExA},
		{EnumFontFamiliesExA, HookEnumFontFamiliesExA},
		{CreateFontA, HookCreateFontA}
	};
	for (const auto& [t, h] : addr) {
		writeHook(t, h);
	}
}

std::optional<PatchPack> PackFromTextFile() {
	std::error_code ec;
	if (!fs::is_directory(SEEN_DATA_DIR, ec)) return std::nullopt;
	PatchPack pack;
	auto loadFile = [](const std::string& file) -> std::optional<std::string> {
			auto opt = readTextFile(file);
			if (!opt) return std::nullopt;
			return transferToGbk(*opt);
		};
	auto asmSeen = [](size_t idx) {
			char buf[MAX_PATH];
			std::snprintf(buf, sizeof(buf), SEEN_DATA_FILE, idx);
			return std::string(buf);
		};

	std::optional<std::string> nameFile = loadFile(NAME_DATA_FILE);
	if (nameFile) pack.name.loadFromTextFile(*nameFile);
	std::optional<std::string> peFile = loadFile(PERES_DATA_FILE);
	if (peFile) pack.res.loadFromTextFile(*peFile);
	std::optional<std::string> fileMap = loadFile(FMAP_DATA_FILE);
	if (fileMap) pack.fileMap.loadFromTextFile(*fileMap);

	pack.text.reserve(SEEN_DATA_NUM);
	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		std::optional<std::string> seenFile = loadFile(asmSeen(i));
		if (seenFile) {
			SeenTextData data;
			data.loadFromTextFile(*seenFile);
			pack.text.push_back(std::move(data));
		}
	}
	return pack;
}

std::optional<std::vector<BYTE>> readBinFile(const std::string& path) {
	try {
		std::ifstream in(fs::path(path), std::ios::binary | std::ios::ate);
		if (!in) return std::nullopt;

		const std::streamoff size = in.tellg();
		if (size < 0) return std::nullopt;

		std::vector<BYTE> data(static_cast<std::size_t>(size));
		in.seekg(0, std::ios::beg);
		if (size > 0) {
			in.read(reinterpret_cast<char*>(data.data()),
				static_cast<std::streamsize>(size));
			if (!in) return std::nullopt;
		}
		return data;
	} catch (...) {
		return std::nullopt;
	}
}

std::optional<std::string> readTextFile(const std::string& path) {
	try {
		std::ifstream in(fs::path(path), std::ios::binary);
		if (!in) return std::nullopt;

		std::string data((std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());
		if (in.bad()) return std::nullopt;
		return data;
	} catch (...) {
		return std::nullopt;
	}
}

static void updateMemoryData(void* address, std::vector<BYTE> bytes) {
	DWORD oldProtect = 0;
	size_t len = sizeof(BYTE) * bytes.size();
	VirtualProtect(address, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy_s(address, len, bytes.data(), len);
	DWORD tmp = 0;
	VirtualProtect(address, len, oldProtect, &tmp);
	FlushInstructionCache(GetCurrentProcess(), address, len);
}

static std::vector<BYTE> assembleCallOp(void* targetFuncAddr, void* hookFuncAddr) {
	std::vector<BYTE> buffer;
	buffer.reserve(5);
	buffer.push_back(0xE8);
	DWORD rva = (DWORD)hookFuncAddr - ((DWORD)targetFuncAddr + 0x5);
	buffer.push_back(static_cast<BYTE>(rva & 0xFF));
	buffer.push_back(static_cast<BYTE>((rva >> 8) & 0xFF));
	buffer.push_back(static_cast<BYTE>((rva >> 16) & 0xFF));
	buffer.push_back(static_cast<BYTE>((rva >> 24) & 0xFF));
	return buffer;
}

static void writeHookWithNop(void* targetFuncAddr, void* hookFuncAddr, unsigned nopLength) {
	std::vector<BYTE> op = assembleCallOp(targetFuncAddr, hookFuncAddr);
	for (size_t i = 0; i < nopLength; ++i) {
		op.push_back(0x90);
	}
	updateMemoryData(targetFuncAddr, op);
}

static void writeHook(void* targetFuncAddr, void* hookFuncAddr) {
	writeHookWithNop(targetFuncAddr, hookFuncAddr, 0);
}

static void skipAsmCode(void* targetAddr, size_t codeLength) {
	std::vector<BYTE> buffer(codeLength, 0x90);
	updateMemoryData(targetAddr, buffer);
}

void TransferBinBytes(std::vector<BYTE>& in) {
	std::transform(in.begin(), in.end(), in.begin(),
		[](BYTE b) { 
			b = (BYTE)(((b & 0x0F) << 4) | ((b & 0xF0) >> 4));
			b = (BYTE)(((b & 0x33) << 2) | ((b & 0xCC) >> 2));
			b = (BYTE)(((b & 0x55) << 1) | ((b & 0xAA) >> 1));
			return b;
		});
}

static std::string transferToGbk(const std::string& str) {
	if (str.empty()) return str;
	int wLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), NULL, 0);
	if (wLen <= 0) {
		return str;
	}
	std::vector<WCHAR> wBuf(wLen);
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), wBuf.data(), wLen);
	int len = WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wBuf.data(), wLen, NULL, NULL, NULL, NULL);
	if (len <= 0) {
		return str;
	}
	std::vector<char> buf(len);
	WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wBuf.data(), wLen, buf.data(), len, NULL, NULL);
	return std::string(buf.data(), buf.size());
}
