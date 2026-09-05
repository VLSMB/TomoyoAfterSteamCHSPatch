#ifndef __VLSMB_HOOK_H
#define __VLSMB_HOOK_H

#include <Windows.h>

void HookInit(HMODULE hDll);
void HookDestroy();
void RunDump();
void PatchHookAfterOpenSeenFile();

#endif
