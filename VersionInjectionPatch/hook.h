#ifndef __VLSMB_HOOK_H
#define __VLSMB_HOOK_H

#include <Windows.h>

void HookInit(HMODULE hDll);
void HookDestroy();
void RunDump();
void PatchHookAfterVMInit();

LRESULT CALLBACK ProxyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI ProxyInvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);

#endif
