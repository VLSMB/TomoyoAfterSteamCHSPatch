#ifndef __VLSMB_STRUCT_H
#define __VLSMB_STRUCT_H

#include <Windows.h>
#include <array>
#include "patch.h"

#define SEEN_DATA_DIR__ "patch"
#define WINDOW_TITLE__ "智代After Steam版汉化补丁 v0.1.0 （汉化文本【TOMO汉化组&KeyFanClub汉化组】v1.2）"
#define C_NAKED_FUNCTION(x) extern "C" void __declspec(naked) x()

constexpr const char* PROCESS_NAME = "RealLiveEn.exe";
constexpr const char* WINDOW_TITLE = WINDOW_TITLE__;
constexpr const char* WINDOW_CLASS_NAME = "VisualArts_System2000_REALLIVEEN";
constexpr const char* MESSAGEBOX_TITLE = "VLSMB";
constexpr const char* PROCESS_FILE_SHA256 = "cbfe30775595145b58af21edca73bc9ed25a54b135adb4702af1d26c1f9aa084";

constexpr const char* SEEN_DATA_DIR = SEEN_DATA_DIR__;
constexpr const char* SEEN_DATA_FILE = SEEN_DATA_DIR__"\\SEEN%04d.txt";
constexpr const char* NAME_DATA_FILE = SEEN_DATA_DIR__"\\name.txt";
constexpr const char* PERES_DATA_FILE = SEEN_DATA_DIR__"\\pe_resource.txt";
constexpr const char* FMAP_DATA_FILE = SEEN_DATA_DIR__"\\file_mapping.txt";
constexpr const char* BIN_DATA_FILE = "SEEN_CHS.TXT";
constexpr const char* REPLACE_FLAG = "replace_flag";
constexpr unsigned SEEN_9900 = 9900;
constexpr const char* SEEN_9900_MODE_FONT = "Consolas";

constexpr const char* GBK_STR = "gbk";
constexpr const char* UTF16_STR = "utf16le";
constexpr const char* NULL_STRATEGY = "fill_null";
constexpr const char* SPACE_STRATEGY = "fill_space";

constexpr unsigned SEEN_DATA_NUM = 10000;
constexpr DWORD BIN_MAGIC = 0x54504C56;
constexpr DWORD TEXT_BIN_MAGIC = 0x4E454553;
constexpr DWORD NAME_BIN_MAGIC = 0x454D414E;
constexpr DWORD PERES_BIN_MAGIC = 0x53524550;
constexpr DWORD FMAP_BIN_MAGIC = 0x50414D46;
constexpr DWORD PACK_BIN_MAGIC = 0x4B434150;

constexpr DWORD CALL_READ_SEEN_HEADER_RVA = 0x526EF;
constexpr DWORD READ_SEEN_HEADER_FUNC_RVA = 0x991B0;
constexpr DWORD READ_SEEN_DATA_FUNC_RVA = 0x8E950;
constexpr DWORD READ_SEEN_DATA_AFTER_HOOK_RVA = 0x8E956;
constexpr DWORD SEEN_HEADER_ENTRY_POINTER_RVA = 0x178BE84;
constexpr DWORD CONSUME_TEXT_IN_QUOTE_MODE_FUNC_RVA = 0xECA80;
constexpr DWORD OPEN_G00_FILE_FUNC_CALLER_RVA = 0xC5371;
constexpr DWORD OPEN_G00_FILE_FUNC_RVA = 0xC4810;
constexpr DWORD HANDLE_SEEN_9900_FUNC_RVA = 0xA4620;
constexpr DWORD INVALIDATE_RECT_IAT_RVA = 0x2064B0;
constexpr DWORD SET_DIB_TO_DEVICE_IAT_RVA = 0x2060A0;
constexpr DWORD RLD3D_PRESENT_IAT_RVA = 0xF32B70;
constexpr DWORD RLD3D_PRESENT_CALLER_RVA = 0x1AE11;
constexpr DWORD GAME_QUIT_MESSAGE_ADDR_RVA = 0xE41A5;
constexpr DWORD FONT_SETTING_TITLE_ADDR_RVA = 0xC6B27;

constexpr DWORD RLVM_STATE_POINTER_RVA = 0x319B6C;
constexpr DWORD RLVM_CONTEXT_POINTET_RVA = 0x576F90;
constexpr DWORD HOOK_SELECT_TEXT_RVA = 0x12657A;
constexpr DWORD HOOK_CHUNK_TEXT_LEN_RVA = 0xEF22C;
constexpr DWORD HOOK_CHUNK_TEXT_RVA = 0xEF29B;
constexpr DWORD HANDLE_NAME_TEXT_CALLER_RVA = 0xEE02A;
constexpr DWORD HANDLE_NAME_TEXT_RVA = 0xEBEC0;
constexpr unsigned SEEN_PATCH_NO = 9836;
constexpr unsigned SEEN_PATCH_OFFSET = 0x2CF0;
constexpr size_t SEEN_PATCH_LEN = 4;

constexpr size_t SET_NOP_ARRAY_SIZE = 33;
constexpr size_t SET_OPCODE_ARRAY_SIZE = 6;
constexpr size_t WINDOW_TITLE_ADDR_RVA_SIZE = 4;
constexpr size_t CONSUME_TEXT_IN_QUOTE_MODE_CALLER_SIZE = 6;

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

extern const DWORD SET_NOP_RVA[SET_NOP_ARRAY_SIZE];
extern const size_t SET_NOP_COUNT[SET_NOP_ARRAY_SIZE];
extern const DWORD SET_OPCODE_RVA[SET_OPCODE_ARRAY_SIZE];
extern const std::vector<BYTE> SET_OPCODE[SET_OPCODE_ARRAY_SIZE];
extern const BYTE SEEN_PATCH_DATA[SEEN_PATCH_LEN];
extern const DWORD CONSUME_TEXT_IN_QUOTE_MODE_CALLER_RVA[CONSUME_TEXT_IN_QUOTE_MODE_CALLER_SIZE];
extern const DWORD WINDOW_TITLE_ADDR_RVA[WINDOW_TITLE_ADDR_RVA_SIZE];
extern const std::string TRASH_BUFFER;

extern const std::vector<BYTE> WINAPI_PATCH_STUB;
extern const char* SHA256_MISMATCH_WARNING;
extern const char* ALPHA_VERSION_WARNING;
extern const char* GAME_QUIT_MESSAGE;
extern const char* WINDOW_TITLE_TEXT;
extern const char* FONT_SETTINGS_TITLE;

extern HWND window_handle;
extern WNDPROC window_proc;
extern void* image_base;
extern RealLiveVMContext* context_pointer;
extern RealLiveVMState* state_pointer;

extern std::unique_ptr<PatchRT> patch_rt;

#endif
