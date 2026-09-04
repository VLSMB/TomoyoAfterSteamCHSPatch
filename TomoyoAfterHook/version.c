#include <windows.h>
#include "hook.h"

#pragma comment(linker, "/EXPORT:GetFileVersionInfoA=_pGetFileVersionInfoA,@1")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoByHandle=_pGetFileVersionInfoByHandle,@2")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoExA=_pGetFileVersionInfoExA,@3")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoExW=_pGetFileVersionInfoExW,@4")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoSizeA=_pGetFileVersionInfoSizeA,@5")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoSizeExA=_pGetFileVersionInfoSizeExA,@6")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoSizeExW=_pGetFileVersionInfoSizeExW,@7")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoSizeW=_pGetFileVersionInfoSizeW,@8")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoW=_pGetFileVersionInfoW,@9")
#pragma comment(linker, "/EXPORT:VerFindFileA=_pVerFindFileA,@10")
#pragma comment(linker, "/EXPORT:VerFindFileW=_pVerFindFileW,@11")
#pragma comment(linker, "/EXPORT:VerInstallFileA=_pVerInstallFileA,@12")
#pragma comment(linker, "/EXPORT:VerInstallFileW=_pVerInstallFileW,@13")
#pragma comment(linker, "/EXPORT:VerLanguageNameA=_pVerLanguageNameA,@14")
#pragma comment(linker, "/EXPORT:VerLanguageNameW=_pVerLanguageNameW,@15")
#pragma comment(linker, "/EXPORT:VerQueryValueA=_pVerQueryValueA,@16")
#pragma comment(linker, "/EXPORT:VerQueryValueW=_pVerQueryValueW,@17")

void pGetFileVersionInfoA();
void pGetFileVersionInfoByHandle();
void pGetFileVersionInfoExA();
void pGetFileVersionInfoExW();
void pGetFileVersionInfoSizeA();
void pGetFileVersionInfoSizeExA();
void pGetFileVersionInfoSizeExW();
void pGetFileVersionInfoSizeW();
void pGetFileVersionInfoW();
void pVerFindFileA();
void pVerFindFileW();
void pVerInstallFileA();
void pVerInstallFileW();
void pVerLanguageNameA();
void pVerLanguageNameW();
void pVerQueryValueA();
void pVerQueryValueW();

#define VERSION_FUNC_COUNT 17
const char* const version_func_name[VERSION_FUNC_COUNT] = {
	"GetFileVersionInfoA", "GetFileVersionInfoByHandle", "GetFileVersionInfoExA", "GetFileVersionInfoExW",
	"GetFileVersionInfoSizeA", "GetFileVersionInfoSizeExA", "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW",
	"GetFileVersionInfoW", "VerFindFileA", "VerFindFileW", "VerInstallFileA", "VerInstallFileW",
	"VerLanguageNameA", "VerLanguageNameW", "VerQueryValueA", "VerQueryValueW"
};
DWORD version_func_addr[VERSION_FUNC_COUNT] = { 0 };
static HMODULE version_module = NULL;

static BOOL initDllProxy(HMODULE hModule);
static void freeDllProxy();

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, PVOID pvReserved) {
	char szCurName[MAX_PATH];
	GetModuleFileNameA(NULL, szCurName, MAX_PATH);
	size_t len = strlen(szCurName);
	size_t offset = strlen(PROCESS_NAME);
	if (len >= offset && _stricmp(szCurName + len - offset, PROCESS_NAME)) {
		return TRUE;
	}
	if (dwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hModule);
		return initDllProxy(hModule);
	} else if (dwReason == DLL_PROCESS_DETACH) {
		freeDllProxy();
	}
	return TRUE;
}

static BOOL initDllProxy(HMODULE hModule) {
	char path[MAX_PATH];
	const size_t len = sizeof(char) * MAX_PATH;
	RtlZeroMemory(path, len);
	GetSystemDirectoryA(path, len);
	lstrcatA(path, "\\version.dll");
	version_module = LoadLibraryA(path);
	if (version_module != NULL) {
		for (size_t i = 0; i < VERSION_FUNC_COUNT; i++) {
			version_func_addr[i] = GetProcAddress(version_module, version_func_name[i]);
		}
	}

	HookInit(hModule);
	return TRUE;
}

static void freeDllProxy() {
	HookDestroy();
	if (version_module != NULL) {
		FreeLibrary(version_module);
	}
}

void ASM_FUNCTION pGetFileVersionInfoA() {
	__asm jmp dword ptr [version_func_addr + 0 * 4];
}

void ASM_FUNCTION pGetFileVersionInfoByHandle() {
	__asm jmp dword ptr[version_func_addr + 1 * 4];
}

void ASM_FUNCTION pGetFileVersionInfoExA() {
	__asm jmp dword ptr[version_func_addr + 2 * 4];
}

void ASM_FUNCTION pGetFileVersionInfoExW() {
	__asm jmp dword ptr[version_func_addr + 3 * 4];
}

void ASM_FUNCTION pGetFileVersionInfoSizeA() {
	__asm jmp dword ptr[version_func_addr + 4 * 4];
}

void ASM_FUNCTION pGetFileVersionInfoSizeExA() {
	__asm jmp dword ptr[version_func_addr + 5 * 4];
}

void ASM_FUNCTION pGetFileVersionInfoSizeExW() {
	__asm jmp dword ptr[version_func_addr + 6 * 4];
}

void ASM_FUNCTION pGetFileVersionInfoSizeW() {
	__asm jmp dword ptr[version_func_addr + 7 * 4];
}

void ASM_FUNCTION pGetFileVersionInfoW() {
	__asm jmp dword ptr[version_func_addr + 8 * 4];
}

void ASM_FUNCTION pVerFindFileA() {
	__asm jmp dword ptr[version_func_addr + 9 * 4];
}

void ASM_FUNCTION pVerFindFileW() {
	__asm jmp dword ptr[version_func_addr + 10 * 4];
}

void ASM_FUNCTION pVerInstallFileA() {
	__asm jmp dword ptr[version_func_addr + 11 * 4];
}

void ASM_FUNCTION pVerInstallFileW() {
	__asm jmp dword ptr[version_func_addr + 12 * 4];
}

void ASM_FUNCTION pVerLanguageNameA() {
	__asm jmp dword ptr[version_func_addr + 13 * 4];
}

void ASM_FUNCTION pVerLanguageNameW() {
	__asm jmp dword ptr[version_func_addr + 14 * 4];
}

void ASM_FUNCTION pVerQueryValueA() {
	__asm jmp dword ptr[version_func_addr + 15 * 4];
}

void ASM_FUNCTION pVerQueryValueW() {
	__asm jmp dword ptr[version_func_addr + 16 * 4];
}
