#ifndef __VLSMB_ASM_H
#define __VLSMB_ASM_H

void HookForDump();
void HookForPatch();
void HookEnumFontFamiliesExA();
void HookCreateFontA();
void HookCreateFileA();
void HookSetWindowTextA();
void HookCreateWindowExA();

void ProxyConsumeTextInQuoteMode();
void JmpSignature();

void ProxyOpenG00File();
void ProxySeen9900Text();
void ProxyRld3dPresent();

void HookSelectText();
void HookChunkTextLength();
void HookChunkText();

#endif
