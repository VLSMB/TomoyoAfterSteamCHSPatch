#include "hook.h"
#include "asm.h"
#include <Windows.h>

void HookInitForDump();
void HookInitForPatch();
void dumpSeenData(ReadSeenDataFuncPtr dataFunc, SeenData* data, int num);
void logError(const char* msg);
void WriteHookCode(DWORD orgAddr, DWORD tarAddr);

BYTE dummy_ctx[64];

void HookInit() {
#ifdef __HOOK_FOR_DUMP
	HookInitForDump();
#else
	HookInitForPatch();
#endif
}

void HookInitForDump() {
	DWORD imageBase = (DWORD)GetModuleHandleA(PROCESS_NAME);
	if (imageBase == NULL) {
		logError("寻找进程基地址失败！");
	}
	WriteHookCode(imageBase + CALL_READ_SEEN_HEADER_RVA, hookForDump);
}

void runDump() {
	DWORD imageBase = (DWORD)GetModuleHandleA(PROCESS_NAME);
	if (imageBase == NULL) {
		logError("寻找进程基地址失败！");
	}
	ReadSeenDataFuncPtr dataFunc = (ReadSeenDataFuncPtr)(imageBase + READ_SEEN_DATA_FUNC_RVA);
	SeenHeaderEntry* entryArray = *(SeenHeaderEntry**)(imageBase + SEEN_HEADER_ENTRY_POINTER_RVA);
	SeenData* data = malloc(sizeof(SeenData));
	if (data == NULL) {
		logError("内存不足");
		return;
	}
	for (int i = 0; i < 10000; i++) {
		if ((entryArray + i)->offset == 0 || (entryArray + i)->size == 0) {
			continue;
		}
		dumpSeenData(dataFunc, data, i);
	}
	free(data);
	MessageBoxA(NULL, "seen解包成功！", "VLSMB", MB_ICONINFORMATION);
	TerminateProcess(GetCurrentProcess(), 0);
}

void HookInitForPatch() {

}

void dumpSeenData(ReadSeenDataFuncPtr dataFunc, SeenData* data, int num) {
	RtlZeroMemory(data, sizeof(SeenData));
	RtlZeroMemory(dummy_ctx, sizeof(dummy_ctx));
	dataFunc(dummy_ctx, data, num, 0);
	char path[MAX_PATH];
	wsprintfA(path, "dump\\SEEN%04d.txt", num);
	CreateDirectoryA("dump", NULL);
	HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD written;
		WriteFile(hFile, data->decompressed_data, data->decompressed_size, &written, NULL);
		CloseHandle(hFile);
	}
}

void logError(const char* msg) {
	MessageBoxA(NULL, msg, "VLSMB", MB_ICONERROR);
}

void WriteHookCode(DWORD orgAddr, DWORD tarAddr)
{
	DWORD oldProtect = 0;
	VirtualProtect((LPVOID)orgAddr, 0x5, PAGE_EXECUTE_READWRITE, &oldProtect);
	DWORD rvaAddr = tarAddr - orgAddr - 0x5;
	BYTE code[5] = { 0xE8,0x90,0x90,0x90,0x90 };
	memcpy_s(&code[1], 4, &rvaAddr, 4);
	memcpy_s((void*)orgAddr, 5, code, 5);
}
