#ifndef __VLSMB_STRUCT_H
#define __VLSMB_STRUCT_H

#include <Windows.h>

#define ASM_FUNCTION __declspec(naked)

#define SEEN_DATA_NUM 10000
#define PROCESS_NAME "RealLiveEn.exe"
#define WINDOW_TITLE "智代后记（Steam版）汉化补丁 v0.0.2-dev"
#define ORIGIN_TITLE_NAME "tomoyo after -It's a Wonderful Life-  English Edition    "
#define MESSAGEBOX_TITLE "VLSMB"
#define PROCESS_FILE_SHA256 "cbfe30775595145b58af21edca73bc9ed25a54b135adb4702af1d26c1f9aa084"
#define SEEN_DATA_FILE "patch\\SEEN%04d.txt"
#define NAME_DATA_FILE "patch\\name.txt"
#define BIN_DATA_FILE "patch.vlpt"
#define SEEN_DATA_DIR "patch"
#define PATCH_MODE_CONFIG_FILE "patch_mode.cfg"
#define EMPTY_NAME "NULL"
#define BIN_MAGIC (DWORD)(0x54504C56)
#define TEXT_BIN_MAGIC (DWORD)(0x4E454553)
#define NAME_BIN_MAGIC (DWORD)(0x454D414E)
#define PACK_BIN_MAGIC (DWORD)(0x4B434150)
#define TEXT_META_MAGIC (BYTE)(0xF0)
#define SHOT_TEXT_SIZE 10

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

typedef enum PatchModeEnum {
    PATCH_RELEASE,
    PATCH_DUMP,
    PATCH_ARCHIVE,
    PATCH_DEBUG,
    PATCH_NONE
} PatchMode;

typedef struct RealLiveSeenHeaderStruct {
    DWORD offset;
    DWORD size;
} RealLiveSeenHeader;

typedef struct RealLiveSeenDataStruct {
    DWORD unknown1;
    DWORD unknown2;
    DWORD unknown3;
    DWORD unknown4;
    DWORD unknown5;
    DWORD unknown6;
    BYTE* decompressed_data;
    DWORD decompressed_size;
} RealLiveSeenData;

typedef struct RealLiveVMStateStruct {
    unsigned seenNo;
} RealLiveVMState;

typedef struct RealLiveVMContextStruct {
    DWORD unknown1;
    DWORD unknown2;
    DWORD unknown3;
    DWORD unknown4;
    DWORD unknown5;
    DWORD unknown6;
    BYTE* vmBase;
    DWORD unknown7;
    BYTE* vmIp;
} RealLiveVMContext;

typedef struct ByteBufferStruct {
    BYTE* pointer;
    size_t size;
} ByteBuffer;

typedef struct SeenPatchDataStruct {
    unsigned index;
    unsigned offset;
    size_t length;
    ByteBuffer name;
    ByteBuffer origin;
    ByteBuffer translated;
} SeenPatchData;

typedef struct SeenPatchDataArrayStruct {
    unsigned seenNo;
    SeenPatchData* pointer;
    size_t size;
} SeenPatchDataArray;

typedef struct NameDataStruct {
    ByteBuffer origin;
    ByteBuffer translated;
} NameData;

typedef struct NameDataArrayStruct {
    NameData* pointer;
    size_t size;
} NameDataArray;

typedef struct SeenDumpDataStruct {
    SeenPatchDataArray textData;
    NameDataArray nameData;
} SeenDumpData;

typedef struct PatchPackStruct {
    SeenPatchDataArray** pText;
    size_t pSize;
    NameDataArray* pName;
} PatchPack;

typedef struct CharacterInfoStruct {
    unsigned seenNo;
    unsigned offset;
    unsigned length;
    BYTE asciiFlag;
    BYTE lastFlag;
    WORD character;
} CharacterInfo;

typedef DWORD(__fastcall* ReadSeenHeaderFuncPtr)(void);
typedef DWORD(__fastcall* ReadSeenDataFuncPtr)(BYTE ctx[64], RealLiveSeenData* out, DWORD scene_no, DWORD flags);
typedef DWORD(__fastcall* DrawSingleCharacterFuncPtr)(DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD character, DWORD a6, DWORD a7, DWORD a8, DWORD a9, DWORD a10, DWORD a11, DWORD a12);
typedef DWORD(__fastcall* ConsumeTextInQuoteModeFuncPtr)(RealLiveVMState* sp, RealLiveVMContext* cp, int byteMode, int a4);

#endif
