#ifndef __VLSMB_HOOK_H
#define __VLSMB_HOOK_H

#include <Windows.h>

#define PROCESS_NAME "RealLive.exe"
#define PROCESS_FILE_SHA256 "cbfe30775595145b58af21edca73bc9ed25a54b135adb4702af1d26c1f9aa084"
#define SEEN_DATA_NUM 10000
#define SEEN_DATA_FILE "patch\\SEEN%04d.txt"
#define SEEN_DATA_DIR "patch"
#define MESSAGEBOX_TITLE "VLSMB"
#define PATCH_MODE_CONFIG_FILE "patch_mode.cfg"

#define CALL_READ_SEEN_HEADER_RVA 0x526EF
#define READ_SEEN_HEADER_FUNC_RVA 0x991B0
#define READ_SEEN_DATA_FUNC_RVA 0x8E950
#define READ_SEEN_DATA_AFTER_HOOK_RVA 0x8E956
#define SEEN_HEADER_ENTRY_POINTER_RVA 0x178BE84
#define DRAW_SINGLE_CHAR_FUNC_RVA 0xC7F10
#define CONSUME_TEXT_IN_QUOTE_MODE_FUNC_RVA 0xECA80
#define CONSUME_TEXT_IN_QUOTE_MODE_CALLER_RVA 0xE9F9D
#define HANDLE_NAME_TEXT_FUNC_RVA 0x17E5A0

typedef enum PatchModeEnum {
    PATCH_RELEASE,
    PATCH_DUMP,
    PATCH_ARCHIVE,
    PATCH_DEBUG,
    PATCH_NONE
} PatchMode;

typedef struct SeenHeaderEntryStruct {
    DWORD offset;
    DWORD size;
} SeenHeaderEntry;

typedef struct SeenDataStruct {
    DWORD unknown1;
    BYTE* raw_data;
    BYTE* buffer;
    DWORD unknown2;
    DWORD unknown3;
    BYTE* code_start;
    BYTE* decompressed_data;
    DWORD decompressed_size;
} SeenData;

typedef DWORD(__fastcall* ReadSeenHeaderFuncPtr)(void);
typedef DWORD(__fastcall* ReadSeenDataFuncPtr)(BYTE ctx[64], SeenData* out, DWORD scene_no, DWORD flags);
typedef DWORD(__fastcall* DrawSingleCharacterFuncPtr)(DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD character, DWORD a6, DWORD a7, DWORD a8, DWORD a9, DWORD a10, DWORD a11, DWORD a12);
typedef DWORD(__fastcall* ConsumeTextInQuoteModeFuncPtr)(void* p1, void* p2, int a3, int a4);
#define GET_VM_IP_POINTER(p) (*(BYTE**)((DWORD)p2 + 32))

void HookInit();
void HookDestroy();
void RunDump();
BYTE* GetSeenPatchData(size_t num);
void PatchHookAfterOpenSeenFile();

#endif

