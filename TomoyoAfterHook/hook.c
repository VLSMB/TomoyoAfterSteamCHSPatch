#include "hook.h"
#include "asm.h"
#include <Windows.h>
#include <stdio.h>
#include <wincrypt.h>

#define SET_NOP_ARRAY_SIZE 23
const DWORD SET_NOP_RVA[SET_NOP_ARRAY_SIZE] = { 
	0xEDC3E, 0xCFC94, 0xEBF6E, 0xEBFA6, 0xA578E, 0x17E6AD, 0xEBBBE, 0xD0F42, 
	0x17E76E, 0xEBA96, 0xEF297, 0xC5F0D, 0xC99AD, 0xC9646, 0xEDF5D, 0xEDBDC,
	0xECE3E, 0x604D6, 0x6062A, 0x13F3BC, 0x13E74A, 0xA519D, 0xE9D01
};
const size_t SET_NOP_COUNT[SET_NOP_ARRAY_SIZE] = { 
	29, 12, 37, 12, 32, 12, 20, 12, 
	20, 12, 15, 15, 12, 12, 20, 20,
	 8, 12, 20, 12, 12, 20, 20
};

BYTE dummy_ctx[64];
BYTE* seen_data_buffer[SEEN_DATA_NUM] = { NULL };
PatchMode patch_mode = PATCH_RELEASE;
int hookProcessAfterVMInitFlag = 0;

void initPatchMode();
char* calculateSHA256();
char singleHexToChar(BYTE b);
DWORD getImageBase();
void initForDump();
void initForPatch();
void dumpSeenData(ReadSeenDataFuncPtr dataFunc, SeenData* data, size_t num);
void logError(const char* msg);
void updateAsmCode(void* address, BYTE* codeArr, size_t len);
void assembleCallOp(BYTE* buffer, size_t len, void* targetFuncAddr, void* hookFuncAddr);
void writeHook(void* targetFuncAddr, void* hookFuncAddr);
void writeHookWithNop(void* targetFuncAddr, void* hookFuncAddr, unsigned nopLength);
void loadSeenPatchData();
void skipAsmCode(DWORD imageBase, DWORD rva, size_t codeLength);

void HookInit() {
	initPatchMode();
	switch (patch_mode) {
	case PATCH_DUMP:
		initForDump();
		break;
	case PATCH_RELEASE:
	case PATCH_DEBUG:
		initForPatch();
		break;
	case PATCH_ARCHIVE:
	case PATCH_NONE:
		break;
	}
}

void PatchHookAfterOpenSeenFile() {
	if (hookProcessAfterVMInitFlag) {
		return;
	}
	hookProcessAfterVMInitFlag = 1;
	switch (patch_mode) {
	case PATCH_DUMP:
		break;
	case PATCH_RELEASE:
	case PATCH_DEBUG:
	{
		const DWORD imageBase = getImageBase();
		DWORD hookAddress = imageBase + READ_SEEN_DATA_FUNC_RVA;
		loadSeenPatchData();
		BYTE callHookOp[6];
		assembleCallOp(callHookOp, 6, hookAddress, HookForPatch);
		callHookOp[5] = 0xC3;
		updateAsmCode(hookAddress, callHookOp, 6);

		writeHook(EnumFontFamiliesExA, HookEnumFontFamiliesExA);
		writeHook(CreateFontA, HookCreateFontA);

		for (size_t i = 0; i < SET_NOP_ARRAY_SIZE; i++) {
			skipAsmCode(imageBase, SET_NOP_RVA[i], SET_NOP_COUNT[i]);
		}

		writeHook(imageBase + CONSUME_TEXT_IN_QUOTE_MODE_CALLER_1_RVA, ProxyConsumeTextInQuoteMode);
		writeHook(imageBase + CONSUME_TEXT_IN_QUOTE_MODE_CALLER_2_RVA, ProxyConsumeTextInQuoteMode);
		writeHookWithNop(imageBase + HANDLE_NAME_TEXT_FUNC_RVA, HookHandleNameText, 1);
	}
		break;
	case PATCH_ARCHIVE:
	case PATCH_NONE:
		break;
	}
}

void HookDestroy() {
	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		if (seen_data_buffer[i] != NULL) {
			free(seen_data_buffer[i]);
		}
	}
}

void initPatchMode() {
	BYTE* sha256 = calculateSHA256();
	if (strcmp(sha256, PROCESS_FILE_SHA256)) {
		int btn = MessageBoxA(NULL, 
			"检测到当前程序与补丁版本不匹配，本补丁是为Steam版《Tomoyo After English Edition》准备的。\r\n"
			"如果继续运行汉化补丁可能会出现未知错误，是否仍然要继续运行汉化补丁？\r\n"
			"（选择“是”则继续启动汉化补丁，选择“否”则关闭补丁运行原版程序）",
			MESSAGEBOX_TITLE, MB_YESNO | MB_ICONWARNING);
		if (btn != IDYES) {
			patch_mode = PATCH_NONE;
			return;
		}
	}
	FILE* fp = fopen(PATCH_MODE_CONFIG_FILE, "rb");
	if (fp == NULL) {
		patch_mode = PATCH_RELEASE;
		return;
	}
	int ch = fgetc(fp);
	fclose(fp);
	if (ch == EOF || ch < '0' || ch > '4') {
		patch_mode = PATCH_RELEASE;
		return;
	}
	patch_mode = (PatchMode)(ch - '0');
}

char* calculateSHA256() {
	HANDLE hFile = CreateFileA(PROCESS_NAME, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	BYTE out_hash[32] = { 0 };
	int flag = 1;

	if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
		if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
			BYTE buffer[8192];
			DWORD bytesRead;
			while (flag && ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
				if (!CryptHashData(hHash, buffer, bytesRead, 0)) {
					flag = 0;
				}
			}
			if (flag) {
				DWORD hashLen = 32;
				flag = CryptGetHashParam(hHash, HP_HASHVAL, out_hash, &hashLen, 0);
			}
			CryptDestroyHash(hHash);
		}
		CryptReleaseContext(hProv, 0);
	}
	CloseHandle(hFile);

	if (flag) {
		char* result = (char*)malloc(65 * sizeof(char));
		char* rp = result;
		if (result == NULL) {
			return NULL;
		}
		for (int i = 0; i < 32; i++) {
			*(rp++) = singleHexToChar((out_hash[i] >> 4) & 0xF);
			*(rp++) = singleHexToChar(out_hash[i] & 0xF);
		}
		*rp = '\0';
		return result;
	} else {
		return NULL;
	}
}

char singleHexToChar(BYTE b) {
	if (b >= 0 && b <= 9) {
		return '0' + b;
	} else if (b < 16) {
		return 'a' + (b - 10);
	} else {
		return '?';
	}
}

DWORD getImageBase() {
	DWORD imageBase = (DWORD)GetModuleHandleA(PROCESS_NAME);
	if (imageBase == NULL) {
		logError("寻找进程基地址失败！");
		TerminateProcess(GetCurrentProcess(), 1);
	}
	return imageBase;
}

void initForDump() {
	DWORD hookAddress = getImageBase() + CALL_READ_SEEN_HEADER_RVA;
	writeHook(hookAddress, HookForDump);
}

void initForPatch() {
	HMODULE hModule = GetModuleHandleA("kernelbase.dll");
	if (hModule == NULL) {
		hModule = LoadLibraryA("kernelbase.dll");
	}
	if (hModule == NULL) {
		ExitProcess(1);
	}
	writeHook(GetProcAddress(hModule, "CreateFileA"), HookCreateFileA);
}

void loadSeenPatchData() {
	char path[MAX_PATH];
	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		wsprintfA(path, SEEN_DATA_FILE, i);
		HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			seen_data_buffer[i] = NULL;
			continue;
		}
		size_t fileSize = GetFileSize(hFile, NULL);
		if (fileSize == INVALID_FILE_SIZE) {
			logError("获取文件大小失败！");
			goto hook_init_patch_fail;
		}
		if (fileSize == 0) {
			seen_data_buffer[i] = NULL;
			goto hook_init_patch_skip;
		}
		seen_data_buffer[i] = (BYTE*)malloc(fileSize * sizeof(BYTE));
		if (seen_data_buffer[i] == NULL) {
			logError("内存不足");
			goto hook_init_patch_fail;
		}
		size_t bytesRead = 0;
		if (!ReadFile(hFile, seen_data_buffer[i], fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
			logError("读取文件失败");
			goto hook_init_patch_fail;
		}
	hook_init_patch_skip:
		CloseHandle(hFile);
		continue;
	hook_init_patch_fail:
		CloseHandle(hFile);
		TerminateProcess(GetCurrentProcess(), 1);
	}
}

BYTE* GetSeenPatchData(size_t num) {
	if (num < 0 || num >= SEEN_DATA_NUM) {
		return NULL;
	}
	return seen_data_buffer[num];
}

void RunDump() {
	DWORD imageBase = (DWORD)GetModuleHandleA(PROCESS_NAME);
	if (imageBase == NULL) {
		logError("寻找进程基地址失败！");
		TerminateProcess(GetCurrentProcess(), 1);
	}
	ReadSeenDataFuncPtr dataFunc = (ReadSeenDataFuncPtr)(imageBase + READ_SEEN_DATA_FUNC_RVA);
	SeenHeaderEntry* entryArray = *(SeenHeaderEntry**)(imageBase + SEEN_HEADER_ENTRY_POINTER_RVA);
	SeenData* data = malloc(sizeof(SeenData));
	if (data == NULL) {
		logError("内存不足");
		TerminateProcess(GetCurrentProcess(), 1);
	}
	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		if ((entryArray + i)->offset == 0 || (entryArray + i)->size == 0) {
			continue;
		}
		dumpSeenData(dataFunc, data, i);
	}
	free(data);
	MessageBoxA(NULL, "seen解包成功！", MESSAGEBOX_TITLE, MB_ICONINFORMATION);
	ExitProcess(0);
}

void dumpSeenData(ReadSeenDataFuncPtr dataFunc, SeenData* data, size_t num) {
	RtlZeroMemory(data, sizeof(SeenData));
	RtlZeroMemory(dummy_ctx, sizeof(dummy_ctx));
	dataFunc(dummy_ctx, data, num, 0);
	__asm {
		add esp, 8
	}
	char path[MAX_PATH];
	wsprintfA(path, SEEN_DATA_FILE, num);
	CreateDirectoryA(SEEN_DATA_DIR, NULL);
	HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD written;
		WriteFile(hFile, data->decompressed_data, data->decompressed_size, &written, NULL);
		CloseHandle(hFile);
	}
}

void logError(const char* msg) {
	MessageBoxA(NULL, msg, MESSAGEBOX_TITLE, MB_ICONERROR);
}

void updateAsmCode(void* address, BYTE* codeArr, size_t len) {
	DWORD oldProtect = 0;
	VirtualProtect(address, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy_s(address, len, codeArr, len);
	DWORD tmp = 0;
	VirtualProtect(address, len, oldProtect, &tmp);
	FlushInstructionCache(GetCurrentProcess(), address, len);
}

void assembleCallOp(BYTE* buffer, size_t len, void* targetFuncAddr, void* hookFuncAddr) {
	RtlFillMemory(buffer, len * sizeof(BYTE), 0x90);
	if (len < 5) {
		return;
	}
	buffer[0] = 0xE8;
	DWORD rva = (DWORD)hookFuncAddr - ((DWORD)targetFuncAddr + 0x5);
	memcpy_s(buffer + 1, 4, &rva, 4);
}

void writeHookWithNop(void* targetFuncAddr, void* hookFuncAddr, unsigned nopLength) {
	size_t len = 5 + nopLength;
	BYTE* callHookOp = (BYTE*)malloc(len);
	if (callHookOp == NULL) {
		logError("内存不足");
		ExitProcess(1);
	}
	RtlFillMemory(callHookOp, len, 0x90);
	assembleCallOp(callHookOp, len, targetFuncAddr, hookFuncAddr);
	updateAsmCode(targetFuncAddr, callHookOp, len);
}

void writeHook(void* targetFuncAddr, void* hookFuncAddr) {
	writeHookWithNop(targetFuncAddr, hookFuncAddr, 0);
}

void skipAsmCode(DWORD imageBase, DWORD rva, size_t codeLength) {
	BYTE* buffer = (BYTE*)malloc(codeLength);
	if (buffer == NULL) {
		logError("内存不足");
		TerminateProcess(GetCurrentProcess(), 1);
		return;
	}
	RtlFillMemory(buffer, codeLength, 0x90);
	if (codeLength >= 5) {
		buffer[0] = 0xE9;
		DWORD offset = codeLength - 0x5;
		memcpy_s(buffer + 1, 4, &offset, 4);
	}
	updateAsmCode(imageBase + rva, buffer, codeLength);
}
