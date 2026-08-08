#ifndef __VLSMB_HOOK_H
#define __VLSMB_HOOK_H

#include <Windows.h>

#define PROCESS_NAME "RealLive.exe"
//#define __HOOK_FOR_DUMP
#define CALL_READ_SEEN_HEADER_RVA 0x526EF
#define READ_SEEN_HEADER_FUNC_RVA 0x991B0
#define READ_SEEN_DATA_FUNC_RVA 0x8E950
#define READ_SEEN_DATA_AFTER_HOOK_RVA 0x8E956
#define SEEN_HEADER_ENTRY_POINTER_RVA 0x178BE84

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

void HookInit();
void HookDestroy();
void RunDump();
BYTE* GetSeenPatchData(size_t num);

#endif

