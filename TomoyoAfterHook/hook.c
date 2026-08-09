#include "hook.h"
#include "asm.h"
#include <Windows.h>

#define SEEN_DATA_NUM 10000
#define SEEN_DATA_FILE "patch\\SEEN%04d.txt"
#define SEEN_DATA_DIR "patch"
#define MESSAGEBOX_TITLE "VLSMB"

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
BYTE* seen_data_buffer[SEEN_DATA_NUM];

DWORD getImageBase();
void initForDump();
void initForPatch();
void dumpSeenData(ReadSeenDataFuncPtr dataFunc, SeenData* data, size_t num);
void logError(const char* msg);
void updateAsmCode(void* address, BYTE* codeArr, size_t len);
void assembleCallOp(BYTE* buffer, size_t len, void* targetFuncAddr, void* hookFuncAddr);
void writeHook(void* targetFuncAddr, void* hookFuncAddr);
void loadSeenPatchData();
void skipAsmCode(DWORD imageBase, DWORD rva, size_t codeLength);

void HookInit() {
#ifdef __HOOK_FOR_DUMP
	initForDump();
#else
	initForPatch();
#endif
}

void HookDestroy() {
#ifndef __HOOK_FOR_DUMP
	for (size_t i = 0; i < SEEN_DATA_NUM; i++) {
		if (seen_data_buffer[i] != NULL) {
			free(seen_data_buffer[i]);
		}
	}
#endif
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
	TerminateProcess(GetCurrentProcess(), 0);
}

void dumpSeenData(ReadSeenDataFuncPtr dataFunc, SeenData* data, size_t num) {
	RtlZeroMemory(data, sizeof(SeenData));
	RtlZeroMemory(dummy_ctx, sizeof(dummy_ctx));
	dataFunc(dummy_ctx, data, num, 0);
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

void writeHook(void* targetFuncAddr, void* hookFuncAddr) {
	BYTE callHookOp[5];
	assembleCallOp(callHookOp, 5, targetFuncAddr, hookFuncAddr);
	updateAsmCode(targetFuncAddr, callHookOp, 5);
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
