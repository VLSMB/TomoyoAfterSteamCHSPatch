#include "hook.h"
#include "define.h"

int main(void) {
	RunPack();
	MessageBoxA(NULL, WINDOW_TITLE"\r\nSUCCESS", " Patch Packer", MB_OK | MB_ICONINFORMATION);
	return 0;
}