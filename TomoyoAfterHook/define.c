#include "define.h"

const DWORD SET_NOP_RVA[SET_NOP_ARRAY_SIZE] = {
	0xEDC3E, 0xCFC94, 0xEBF6E, 0xEBFA6, 0xA578E, 0x17E6AD, 0xEBBBE, 0xD0F42,
	0x17E76E, 0xEBA96, 0xEF297, 0xC5F0D, 0xC99AD, 0xC9646, 0xEDF5D, 0xEDBDC,
	0xECE3E, 0x604D6, 0x6062A, 0x13F3BC, 0x13E74A, 0xA519D, 0xE9D01, 0xE976B,
	0xEF22A, 0x126576, 0x1264DB, 0x1262EC, 0x126F24, 0x1BD0D, 0xECDEA, 0x1266E3
};

const size_t SET_NOP_COUNT[SET_NOP_ARRAY_SIZE] = {
	29, 12, 37, 12, 32, 12, 20, 12,
	20, 12, 15, 15, 12, 12, 20, 20,
	 8, 12, 20, 12, 12, 20, 20, 12,
	12, 12, 12,  8, 12, 15,  8,  8
};

const BYTE WINAPI_PATCH_STUB[5] = { 0x8B, 0xFF, 0x55, 0x8B, 0xEC };

const char* SHA256_MISMATCH_WARNING = 
"检测到当前程序与补丁版本不匹配，本补丁是为Steam版《Tomoyo After English Edition》准备的。\r\n"
"如果继续运行汉化补丁可能会出现未知错误，是否仍然要继续运行汉化补丁？\r\n"
"（选择“是”则继续启动汉化补丁，选择“否”则关闭补丁运行原版程序）";

const char* ALPHA_VERSION_WARNING = WINDOW_TITLE
"\r\n本补丁不是完整的汉化补丁，仅用于补丁可行性验证。\r\n"
"汉化补丁完成进度可以关注：https://github.com/VLSMB/TomoyoAfterSteamCHSPatch\r\n"
"本补丁仅用于学习研究用途，禁止用于一切商业活动。";

BYTE* debug_flag_pointer = NULL;
