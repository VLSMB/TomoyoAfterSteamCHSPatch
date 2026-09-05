#ifndef __VLSMB_ASM_H
#define __VLSMB_ASM_H

void HookForDump();
void HookForPatch();
void HookEnumFontFamiliesExA();
void HookCreateFontA();
void HookCreateFileA();
void HookSetWindowTextA();

void ProxyConsumeTextInQuoteMode();
void HookHandleInstantText();

#endif
