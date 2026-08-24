#ifndef __VLSMB_HOOK_H
#define __VLSMB_HOOK_H

#include <Windows.h>
#include "data.h"
#include "resource.h"

#define PROCESS_FILE_SHA256 "cbfe30775595145b58af21edca73bc9ed25a54b135adb4702af1d26c1f9aa084"
#define SEEN_DATA_FILE "patch\\SEEN%04d.txt"
#define NAME_DATA_FILE "patch\\name.txt"
#define BIN_DATA_FILE "patch.vlpt"
#define SEEN_DATA_DIR "patch"
#define PATCH_MODE_CONFIG_FILE "patch_mode.cfg"

#define CALL_READ_SEEN_HEADER_RVA 0x526EF
#define READ_SEEN_HEADER_FUNC_RVA 0x991B0
#define READ_SEEN_DATA_FUNC_RVA 0x8E950
#define READ_SEEN_DATA_AFTER_HOOK_RVA 0x8E956
#define SEEN_HEADER_ENTRY_POINTER_RVA 0x178BE84
#define DRAW_SINGLE_CHAR_FUNC_RVA 0xC7F10
#define CONSUME_TEXT_IN_QUOTE_MODE_FUNC_RVA 0xECA80
#define CONSUME_TEXT_IN_QUOTE_MODE_CALLER_1_RVA 0xE9F9D
#define CONSUME_TEXT_IN_QUOTE_MODE_CALLER_2_RVA 0xE9D5B
#define CONSUME_TEXT_IN_QUITE_MODE_CALLER_3_RVA 0xE9793
#define HANDLE_INSTANT_TEXT_FUNC_RVA 0xEBEC0
#define REALLIVE_DEBUG_MODE_FLAG_RVA 0x302FC0

typedef DWORD(__fastcall* ReadSeenHeaderFuncPtr)(void);
typedef DWORD(__fastcall* ReadSeenDataFuncPtr)(BYTE ctx[64], RealLiveSeenData* out, DWORD scene_no, DWORD flags);
typedef DWORD(__fastcall* DrawSingleCharacterFuncPtr)(DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD character, DWORD a6, DWORD a7, DWORD a8, DWORD a9, DWORD a10, DWORD a11, DWORD a12);
typedef DWORD(__fastcall* ConsumeTextInQuoteModeFuncPtr)(RealLiveVMState* sp, RealLiveVMContext* cp, int byteMode, int a4);

void HookInit(HMODULE hDll);
void HookDestroy();
void RunDump();
void PatchHookAfterOpenSeenFile();

#endif
