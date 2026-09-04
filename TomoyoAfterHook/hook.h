#ifndef __VLSMB_HOOK_H
#define __VLSMB_HOOK_H

#include "data.h"
#include "resource.h"
#include "struct.h"

void HookInit(HMODULE hDll);
void HookDestroy();
void RunDump();
void PatchHookAfterOpenSeenFile();

#endif
