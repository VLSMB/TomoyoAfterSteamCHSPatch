#ifndef __VLSMB_ASM_H
#define __VLSMB_ASM_H

#include <Windows.h>
#include "hook.h"

void HookForDump();
void HookForPatch();
void HookEnumFontFamiliesExA();
void HookCreateFontA();
void HookCreateFileA();

void ProxyConsumeTextInQuoteMode();
void HookHandleNameText();

#endif
