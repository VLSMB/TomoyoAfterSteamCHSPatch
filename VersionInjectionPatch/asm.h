#ifndef __VLSMB_ASM_H
#define __VLSMB_ASM_H

#include <Windows.h>

#define C_NAKED_FUNCTION(x) extern "C" void __declspec(naked) x()
constexpr const char* PROCESS_NAME = "RealLiveEn.exe";

extern "C" {
	void HookForPatch();
	void HookEnumFontFamiliesExA();
	void HookCreateFontA();
	void HookCreateFileA();
	void HookCreateWindowExA();
	void HookAfterCreateWindowExA();

	void ProxyConsumeTextInQuoteMode();
	void JmpSignature();

	void ProxyOpenG00File();
	void ProxySeen9900Text();
	void ProxyRld3dPresent();

	void HookSelectText();
	void HookChunkTextLength();
	void HookChunkText();
	void HookNameText();

	LRESULT CALLBACK ProxyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept;
	BOOL WINAPI ProxyInvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase) noexcept;
	int WINAPI ProxySetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD w, DWORD h, int xSrc,
		int ySrc, UINT StartScan, UINT cLines, const void* lpvBits, const BITMAPINFO* lpbmi, UINT ColorUse) noexcept;
}

#endif
