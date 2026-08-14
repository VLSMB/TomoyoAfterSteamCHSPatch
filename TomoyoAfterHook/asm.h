#ifndef __VLSMB_ASM_H
#define __VLSMB_ASM_H

#include <Windows.h>
#include "hook.h"

#define ASM_FUNCTION __declspec(naked)

void HookForDump();
void HookForPatch();
void HookEnumFontFamiliesExA();
void HookCreateFontA();

void ProxyConsumeTextInQuoteMode();
void HookHandleNameText();

#endif
