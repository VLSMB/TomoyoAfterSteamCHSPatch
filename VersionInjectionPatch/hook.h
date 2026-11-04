#ifndef __VLSMB_HOOK_H
#define __VLSMB_HOOK_H

#include <Windows.h>

void HookInit(HMODULE hDll);
void HookDestroy();
void RunDump();
void RunPack();
void PatchHookAfterVMInit();

LRESULT CALLBACK ProxyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI ProxyInvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
int WINAPI ProxySetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD w, DWORD h, int xSrc,
    int ySrc, UINT StartScan, UINT cLines, const void* lpvBits, const BITMAPINFO* lpbmi, UINT ColorUse);

#endif
