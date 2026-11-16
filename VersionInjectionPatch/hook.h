#ifndef __VLSMB_HOOK_H
#define __VLSMB_HOOK_H

#include <Windows.h>
#include <optional>
#include "data.h"

void HookInit(HMODULE hDll);
void HookDestroy();
std::optional<PatchPack> PackFromTextFile();
void TransferBinBytes(std::vector<BYTE>& in);
void PatchHookAfterVMInit();

#endif
