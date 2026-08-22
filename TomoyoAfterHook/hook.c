#include "hook.h"
#include "asm.h"
#include "data.h"
#include <Windows.h>
#include <stdio.h>
#include <wincrypt.h>

#define SET_NOP_ARRAY_SIZE 32
static const DWORD SET_NOP_RVA[SET_NOP_ARRAY_SIZE] = {
	0xEDC3E, 0xCFC94, 0xEBF6E, 0xEBFA6, 0xA578E, 0x17E6AD, 0xEBBBE, 0xD0F42, 
	0x17E76E, 0xEBA96, 0xEF297, 0xC5F0D, 0xC99AD, 0xC9646, 0xEDF5D, 0xEDBDC,
	0xECE3E, 0x604D6, 0x6062A, 0x13F3BC, 0x13E74A, 0xA519D, 0xE9D01, 0xE976B,
	0xEF22A, 0x126576, 0x1264DB, 0x1262EC, 0x126F24, 0x1BD0D, 0xECDEA, 0x1266E3
};

static const size_t SET_NOP_COUNT[SET_NOP_ARRAY_SIZE] = {
	29, 12, 37, 12, 32, 12, 20, 12, 
	20, 12, 15, 15, 12, 12, 20, 20,
	 8, 12, 20, 12, 12, 20, 20, 12,
	12, 12, 12,  8, 12, 15,  8,  8
};

static ByteBuffer byte_buffer_array[SEEN_DATA_NUM] = { 0 };
static BYTE dummy_ctx[64];
static PatchMode patch_mode = PATCH_RELEASE;
static BOOL hook_process_after_vm_init_flag = FALSE;

static void initPatchMode();
static char* calculateSHA256();
static char singleHexToChar(BYTE b);
static DWORD getImageBase();
static void initForDump();
static void initPatchHook();
static void initForArchive();
static void assemblePatchPackFromTextFile(PatchPack* out);
static void loadPatchPackFromResource(HMODULE hDll, PatchPack* out);
static void dumpSeenData(ReadSeenDataFuncPtr dataFunc, RealLiveSeenData* data, size_t num, SeenDumpData* out);
static BOOL readFile(const char* fileName, ByteBuffer* out);
static void saveFile(const char* fileName, ByteBuffer buffer);
static void logError(const char* msg);
static void updateAsmCode(void* address, BYTE* codeArr, size_t len);
static void assembleCallOp(BYTE* buffer, size_t len, void* targetFuncAddr, void* hookFuncAddr);
static void writeHook(void* targetFuncAddr, void* hookFuncAddr);
static void writeHookWithNop(void* targetFuncAddr, void* hookFuncAddr, unsigned nopLength);
static void skipAsmCode(DWORD imageBase, DWORD rva, size_t codeLength);

void HookInit(HMODULE hDll) {
	initPatchMode();
	MessageBoxA(NULL, 
		"本补丁不是完整的汉化补丁，仅用于补丁可行性验证，没有汉化多少文本。\r\n"
		"汉化补丁完成进度可以关注：https://github.com/VLSMB/TomoyoAfterSteamCHSPatch\r\n"
		"本补丁仅用于学习研究用途，禁止用于一切商业活动。"
		, MESSAGEBOX_TITLE, MB_ICONWARNING);
	PatchPack pack;
	RtlZeroMemory(&pack, sizeof(PatchPack));
	switch (patch_mode) {
	case PATCH_DUMP:
		initForDump();
		break;
	case PATCH_RELEASE:
		loadPatchPackFromResource(hDll, &pack);
		goto patch;
	case PATCH_DEBUG:
		assemblePatchPackFromTextFile(&pack);
		goto patch;
	case PATCH_ARCHIVE:
		initForArchive();
		break;
	case PATCH_NONE:
		break;
	}
	return;
patch:
	InitPatchData(&pack);
	initPatchHook();
}

void PatchHookAfterOpenSeenFile() {
	if (hook_process_after_vm_init_flag) {
		return;
	}
	hook_process_after_vm_init_flag = TRUE;
	if (patch_mode != PATCH_RELEASE && patch_mode != PATCH_DEBUG) {
		return;
	}

	const DWORD imageBase = getImageBase();
	DWORD hookAddress = imageBase + READ_SEEN_DATA_FUNC_RVA;
	BYTE callHookOp[6];
	assembleCallOp(callHookOp, 6, hookAddress, HookForPatch);
	callHookOp[5] = 0xC3;
	updateAsmCode(hookAddress, callHookOp, 6);

	writeHook(EnumFontFamiliesExA, HookEnumFontFamiliesExA);
	writeHook(CreateFontA, HookCreateFontA);
	writeHook(SetWindowTextA, HookSetWindowTextA);

	for (size_t i = 0; i < SET_NOP_ARRAY_SIZE; i++) {
		skipAsmCode(imageBase, SET_NOP_RVA[i], SET_NOP_COUNT[i]);
	}
	writeHook(imageBase + CONSUME_TEXT_IN_QUOTE_MODE_CALLER_1_RVA, ProxyConsumeTextInQuoteMode);
	writeHook(imageBase + CONSUME_TEXT_IN_QUOTE_MODE_CALLER_2_RVA, ProxyConsumeTextInQuoteMode);
	writeHook(imageBase + CONSUME_TEXT_IN_QUITE_MODE_CALLER_3_RVA, ProxyConsumeTextInQuoteMode);
	writeHookWithNop(imageBase + HANDLE_INSTANT_TEXT_FUNC_RVA, HookHandleInstantText, 1);
}

void HookDestroy() {
	CleanPatchData();
}

static void initPatchMode() {
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

static char* calculateSHA256() {
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

static char singleHexToChar(BYTE b) {
	if (b >= 0 && b <= 9) {
		return '0' + b;
	} else if (b < 16) {
		return 'a' + (b - 10);
	} else {
		return '?';
	}
}

static DWORD getImageBase() {
	DWORD imageBase = (DWORD)GetModuleHandleA(PROCESS_NAME);
	if (imageBase == NULL) {
		logError("寻找进程基地址失败！");
		TerminateProcess(GetCurrentProcess(), 1);
	}
	return imageBase;
}

static void initForDump() {
	DWORD hookAddress = getImageBase() + CALL_READ_SEEN_HEADER_RVA;
	writeHook(hookAddress, HookForDump);
}

static void initPatchHook() {
	HMODULE hModule = GetModuleHandleA("kernelbase.dll");
	if (hModule == NULL) {
		hModule = LoadLibraryA("kernelbase.dll");
	}
	if (hModule == NULL) {
		ExitProcess(1);
	}
	writeHook(GetProcAddress(hModule, "CreateFileA"), HookCreateFileA);
}

static void initForArchive() {
	PatchPack pack;
	assemblePatchPackFromTextFile(&pack);
	ByteBuffer buffer;
	RtlZeroMemory(&buffer, sizeof(ByteBuffer));
	PatchPackToBinFile(&pack, &buffer);
	saveFile(BIN_DATA_FILE, buffer);
	FreePatchPack(&pack);
	MessageBoxA(NULL, "打包成功！", MESSAGEBOX_TITLE, MB_OK | MB_ICONINFORMATION);
	ExitProcess(0);
}

static void assemblePatchPackFromTextFile(PatchPack* out) {
	ByteBuffer buffer;
	char path[MAX_PATH];
	RtlZeroMemory(out, sizeof(PatchPack));
	RtlZeroMemory(&buffer, sizeof(ByteBuffer));
	RtlZeroMemory(path, MAX_PATH * sizeof(char));
	size_t seenCount = 0;
	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		wsprintfA(path, SEEN_DATA_FILE, i);
		if (readFile(path, &buffer)) {
			byte_buffer_array[seenCount++] = buffer;
		}
	}
	out->pSize = seenCount;
	out->pText = (SeenPatchDataArray**)malloc(sizeof(SeenPatchDataArray*) * seenCount);
	out->pName = (NameDataArray*)malloc(sizeof(NameDataArray));
	BOOL initFlag = out->pText != NULL && out->pName != NULL;
	if (initFlag) {
		RtlZeroMemory(out->pText, sizeof(SeenPatchDataArray*) * seenCount);
		RtlZeroMemory(out->pName, sizeof(NameDataArray));
	}
	for (size_t i = 0; i < seenCount && initFlag; i++) {
		out->pText[i] = (SeenPatchDataArray*)malloc(sizeof(SeenPatchDataArray));
		initFlag = out->pText[i] != NULL;
	}
	if (!initFlag) {
		FreePatchPack(out);
		logError("内存不足");
		ExitProcess(1);
	}
	for (size_t i = 0; i < seenCount; i++) {
		TextFileToTextData(&byte_buffer_array[i], out->pText[i]);
	}
	if (readFile(NAME_DATA_FILE, &buffer)) {
		TextFileToNameData(&buffer, out->pName);
	}
	for (size_t i = 0; i < seenCount; i++) {
		FreeByteBuffer(&byte_buffer_array[i]);
	}
}

static void loadPatchPackFromResource(HMODULE hDll, PatchPack* out) {
	HRSRC hRes = FindResourceA(hDll, MAKEINTRESOURCEA(IDR_PATCH_BIN), RT_RCDATA);
	if (hRes == NULL) {
		goto fail;
	}
	HGLOBAL hData = LoadResource(hDll, hRes);
	if (hData == NULL) {
		goto fail;
	}
	BYTE* pData = (BYTE*)LockResource(hData);
	DWORD size = SizeofResource(hDll, hRes);
	ByteBuffer buffer;
	RtlZeroMemory(&buffer, sizeof(ByteBuffer));
	buffer.pointer = pData;
	buffer.size = size;
	BinFileToPatchPack(&buffer, out);
	return;
fail:
	logError("读取资源失败！");
	ExitProcess(2);
}

void RunDump() {
	DWORD imageBase = getImageBase();
	ReadSeenDataFuncPtr dataFunc = (ReadSeenDataFuncPtr)(imageBase + READ_SEEN_DATA_FUNC_RVA);
	RealLiveSeenHeader* entryArray = *(RealLiveSeenHeader**)(imageBase + SEEN_HEADER_ENTRY_POINTER_RVA);
	RealLiveSeenData data;
	SeenDumpData** pDumpData = (SeenDumpData**)malloc(SEEN_DATA_NUM * sizeof(SeenDumpData*));
	if (pDumpData == NULL) {
		ExitProcess(1);
	}
	RtlZeroMemory(pDumpData, SEEN_DATA_NUM * sizeof(SeenDumpData*));
	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		if ((entryArray + i)->offset == 0 || (entryArray + i)->size == 0) {
			continue;
		}
		pDumpData[i] = (SeenDumpData*)malloc(sizeof(SeenDumpData));
		if (pDumpData[i] == NULL) {
			ExitProcess(1);
		}
		RtlZeroMemory(pDumpData[i], sizeof(SeenDumpData));
		RtlZeroMemory(&data, sizeof(RealLiveSeenData));
		dumpSeenData(dataFunc, &data, i, pDumpData[i]);
		pDumpData[i]->textData.seenNo = i;
	}

	char path[MAX_PATH];
	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		if (pDumpData[i] == NULL) continue;
		ByteBuffer buffer;
		RtlZeroMemory(&buffer, sizeof(ByteBuffer));
		if (pDumpData[i]->textData.size > 0) {
			TextDataToTextFile(&pDumpData[i]->textData, &buffer);
			wsprintfA(path, SEEN_DATA_FILE, i);
			saveFile(path, buffer);
			FreeByteBuffer(&buffer);
		}
	}
	NameDataArray** pNameDataArray = (NameDataArray**)malloc(SEEN_DATA_NUM * sizeof(NameDataArray*));
	if (pNameDataArray == NULL) {
		ExitProcess(1);
	}

	size_t pSize = 0;
	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		if (pDumpData[i] == NULL) continue;
		pNameDataArray[pSize++] = &pDumpData[i]->nameData;
	}
	NameDataArray* nameData = MergeNameDataArray(pNameDataArray, pSize);
	ByteBuffer buffer;
	RtlZeroMemory(&buffer, sizeof(ByteBuffer));
	NameDataToTextFile(nameData, &buffer);
	saveFile(NAME_DATA_FILE, buffer);

	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		FreeSeenDumpData(pDumpData[i]);
		free(pDumpData[i]);
	}
	FreeNameDataArray(nameData);
	free(nameData);
	free(pDumpData);
	free(pNameDataArray);
	MessageBoxA(NULL, "seen解包成功！", MESSAGEBOX_TITLE, MB_ICONINFORMATION);
	ExitProcess(0);
}

static void dumpSeenData(ReadSeenDataFuncPtr dataFunc, RealLiveSeenData* data, size_t num, SeenDumpData* out) {
	RtlZeroMemory(data, sizeof(RealLiveSeenData));
	RtlZeroMemory(dummy_ctx, sizeof(dummy_ctx));
	dataFunc(dummy_ctx, data, num, 0);
	__asm {
		add esp, 8
	}
	DumpSeenData(data, out);
}

static BOOL readFile(const char* fileName, ByteBuffer* out) {
	RtlZeroMemory(out, sizeof(ByteBuffer));

	HANDLE hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return FALSE;
	}
	size_t fileSize = GetFileSize(hFile, NULL);
	if (fileSize == 0) {
		CloseHandle(hFile);
		return FALSE;
	}
	out->size = fileSize;
	out->pointer = (BYTE*)malloc(fileSize * sizeof(BYTE));
	if (out->pointer == NULL) {
		CloseHandle(hFile);
		return FALSE;
	}

	DWORD bytesRead;
	if (!ReadFile(hFile, out->pointer, fileSize, &bytesRead, NULL)) {
		free(out->pointer);
		RtlZeroMemory(out, sizeof(ByteBuffer));
		CloseHandle(hFile);
		return FALSE;
	}

	if (bytesRead != out->size) {
		free(out->pointer);
		RtlZeroMemory(out, sizeof(ByteBuffer));
		CloseHandle(hFile);
		return FALSE;
	}

	CloseHandle(hFile);
	return TRUE;
}

static void saveFile(const char* fileName, ByteBuffer buffer) {
	CreateDirectoryA(SEEN_DATA_DIR, NULL);
	HANDLE hFile = CreateFileA(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		logError("文件创建失败");
		ExitProcess(2);
	}
	DWORD written = 0;
	WriteFile(hFile, buffer.pointer, buffer.size, &written, NULL);
	CloseHandle(hFile);
}

static void logError(const char* msg) {
	MessageBoxA(NULL, msg, MESSAGEBOX_TITLE, MB_ICONERROR);
}

static void updateAsmCode(void* address, BYTE* codeArr, size_t len) {
	DWORD oldProtect = 0;
	VirtualProtect(address, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy_s(address, len, codeArr, len);
	DWORD tmp = 0;
	VirtualProtect(address, len, oldProtect, &tmp);
	FlushInstructionCache(GetCurrentProcess(), address, len);
}

static void assembleCallOp(BYTE* buffer, size_t len, void* targetFuncAddr, void* hookFuncAddr) {
	RtlFillMemory(buffer, len * sizeof(BYTE), 0x90);
	if (len < 5) {
		return;
	}
	buffer[0] = 0xE8;
	DWORD rva = (DWORD)hookFuncAddr - ((DWORD)targetFuncAddr + 0x5);
	memcpy_s(buffer + 1, 4, &rva, 4);
}

static void writeHookWithNop(void* targetFuncAddr, void* hookFuncAddr, unsigned nopLength) {
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

static void writeHook(void* targetFuncAddr, void* hookFuncAddr) {
	writeHookWithNop(targetFuncAddr, hookFuncAddr, 0);
}

static void skipAsmCode(DWORD imageBase, DWORD rva, size_t codeLength) {
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
