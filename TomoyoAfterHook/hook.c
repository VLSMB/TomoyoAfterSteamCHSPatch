#include "hook.h"
#include "asm.h"
#include <Windows.h>

DWORD getImageBase();
void initForDump();
void initForPatch();
void dumpSeenData(ReadSeenDataFuncPtr dataFunc, SeenData* data, int num);
void logError(const char* msg);
void writeHookCode(DWORD orgAddr, DWORD tarAddr);
void loadSeenPatchData();

#define SEEN_DATA_NUM 10000
#define SEEN_DATA_FILE "patch\\SEEN%04d.txt"
#define SEEN_DATA_DIR "patch"
#define MESSAGEBOX_TITLE "VLSMB"

BYTE dummy_ctx[64];
BYTE* seen_data_buffer[SEEN_DATA_NUM];

void HookInit() {
#ifdef __HOOK_FOR_DUMP
	initForDump();
#else
	initForPatch();
#endif
}

void HookDestroy() {
	for (int i = 0; i < SEEN_DATA_NUM; i++) {
		if (seen_data_buffer[i] != NULL) {
			free(seen_data_buffer[i]);
		}
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
	DWORD imageBase = getImageBase();
	writeHookCode(imageBase + CALL_READ_SEEN_HEADER_RVA, HookForDump);
}

void initForPatch() {
	DWORD imageBase = getImageBase();
	loadSeenPatchData();
}

void loadSeenPatchData() {
	char path[MAX_PATH];
	for (int i = 0; i < SEEN_DATA_NUM; i++) {
		wsprintfA(path, SEEN_DATA_FILE, i);
		HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			seen_data_buffer[i] = NULL;
			continue;
		}
		int fileSize = GetFileSize(hFile, NULL);
		if (fileSize == INVALID_FILE_SIZE) {
			logError("获取文件大小失败！");
			goto hook_init_patch_fail;
		}
		seen_data_buffer[i] = (BYTE*)malloc(fileSize * sizeof(BYTE));
		if (seen_data_buffer[i] == NULL) {
			logError("内存不足");
			goto hook_init_patch_fail;
		}
		int bytesRead = 0;
		if (!ReadFile(hFile, seen_data_buffer[i], fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
			logError("读取文件失败");
			goto hook_init_patch_fail;
		}
		CloseHandle(hFile);
		continue;
	hook_init_patch_fail:
		CloseHandle(hFile);
		TerminateProcess(GetCurrentProcess(), 1);
	}
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
	for (int i = 0; i < SEEN_DATA_NUM; i++) {
		if ((entryArray + i)->offset == 0 || (entryArray + i)->size == 0) {
			continue;
		}
		dumpSeenData(dataFunc, data, i);
	}
	free(data);
	MessageBoxA(NULL, "seen解包成功！", MESSAGEBOX_TITLE, MB_ICONINFORMATION);
	TerminateProcess(GetCurrentProcess(), 0);
}

void dumpSeenData(ReadSeenDataFuncPtr dataFunc, SeenData* data, int num) {
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

void writeHookCode(DWORD orgAddr, DWORD tarAddr)
{
	DWORD oldProtect = 0;
	VirtualProtect((LPVOID)orgAddr, 0x5, PAGE_EXECUTE_READWRITE, &oldProtect);
	DWORD rvaAddr = tarAddr - orgAddr - 0x5;
	BYTE code[5] = { 0xE8,0x90,0x90,0x90,0x90 };
	memcpy_s(&code[1], 4, &rvaAddr, 4);
	memcpy_s((void*)orgAddr, 5, code, 5);
}
