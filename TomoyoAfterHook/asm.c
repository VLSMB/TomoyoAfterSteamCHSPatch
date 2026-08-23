#include "asm.h"

#define ASM_FUNCTION __declspec(naked)

static void WINAPI handleSeenDataPatch(RealLiveSeenData* ptr, unsigned num);
static void WINAPI beforeConsumeTextHook(RealLiveVMState* sp, RealLiveVMContext* cp, int* byteMode, int a4);
static void WINAPI afterConsumeTextHook(RealLiveVMState* sp, RealLiveVMContext* cp, int* byteMode, int a4);
static BOOL WINAPI checkFileIsSeen(const char* fileName);
static const char* const WINAPI getTranslatedText(const char* const text);
static void WINAPI hookWindowTitle(HWND hWnd, const char** title);

void ASM_FUNCTION HookForDump() {
	__asm {
		push offset hook_for_dump
		sub esp, 4
		pushad
		pushfd
	}
	GetModuleHandleA(PROCESS_NAME);
	__asm {
		add eax, READ_SEEN_HEADER_FUNC_RVA
		mov[esp + 36], eax

		popfd
		popad
		ret
	}
hook_for_dump:
	__asm {
		pushad
		pushfd
	}
	RunDump();
	__asm {
		popfd
		popad
		ret
	}
}

void ASM_FUNCTION HookForPatch() {
	__asm {
		mov eax, 0
		mov[edx + 28], eax
	}
	__asm {
		push edx
		mov eax, [esp + 16]
		push eax
		mov eax, [esp + 16]
		push eax

		sub esp, 4
		pushad
		pushfd
	}
	GetModuleHandleA(PROCESS_NAME);
	__asm {
		add eax, READ_SEEN_DATA_AFTER_HOOK_RVA
		mov[esp + 36], eax
		popfd
		popad
		pop eax

		push offset hook_for_patch
		push ebp
		mov ebp, esp
		and esp, 0xFFFFFFF8
		jmp eax
	}
hook_for_patch:
	__asm {
		pushfd
		pushad
		mov eax, [esp + 36]
		push eax
		mov eax, [esp + 48]
		push eax
		call handleSeenDataPatch
		popad
		popfd
		add esp, 12
		ret
	}
}

static void WINAPI handleSeenDataPatch(RealLiveSeenData* ptr, unsigned num) {
	UpdateSeenBuffer(ptr->decompressed_data, num);
}

void ASM_FUNCTION HookEnumFontFamiliesExA() {
	__asm {
		pop eax
		push ebx
		mov ebx, [esp + 12]
		mov byte ptr[ebx + 23], 134
		pop ebx
		mov edi, edi
		push ebp
		mov ebp, esp
		jmp eax
	}
}

void ASM_FUNCTION HookCreateFontA() {
	__asm {
		pop eax
		mov dword ptr ss : [esp + 36] , 134
		mov edi, edi
		push ebp
		mov ebp, esp
		jmp eax
	}
}

void ASM_FUNCTION HookCreateFileA() {
	_asm {
		mov eax, [esp + 8]
		push eax
		call checkFileIsSeen
		test eax, eax
		je __seen_file_ret
	}
	__asm {
		pushfd
		pushad
		call PatchHookAfterOpenSeenFile
		popad
		popfd
	}
__seen_file_ret:
	__asm {
		pop eax
		mov edi, edi
		push ebp
		mov ebp, esp
		jmp eax
	}
}

const char* origin_title = "tomoyo after -It's a Wonderful Life-  English Edition    ";
const char* new_title = "Tomoyo After English Edition ºº»¯°æ v0.0.1";
HWND window_handle = NULL;

void ASM_FUNCTION HookSetWindowTextA() {
	__asm {
		lea eax, [esp + 12]
		push eax
		mov eax, [esp + 12]
		push eax
		call hookWindowTitle
	}
	__asm {
		pop eax
		mov edi, edi
		push ebp
		mov ebp, esp
		jmp eax
	}
}

static void WINAPI hookWindowTitle(HWND hWnd, const char** title) {
	if (window_handle == NULL) {
		if (strcmp(*title, origin_title) == 0) {
			window_handle = hWnd;
		}
	}
	if (window_handle == hWnd) {
		// TODO
	}
}

static BOOL WINAPI checkFileIsSeen(const char* fileName) {
	const int len = strlen("SEEN.TXT");
	if (strlen(fileName) < len) {
		return FALSE;
	}
	const char* p = fileName;
	while (*(++p));
	p -= len;
	return lstrcmpiA("SEEN.TXT", p) == 0;
}

void ASM_FUNCTION ProxyConsumeTextInQuoteMode() {
	__asm {
		push ebp
		mov ebp, esp
		sub esp, 20
		pushad
		pushfd
	}
	GetModuleHandleA(PROCESS_NAME);
	__asm {
		add eax, CONSUME_TEXT_IN_QUOTE_MODE_FUNC_RVA
		mov [ebp - 4], eax
		popfd
		popad
	}
	__asm {
		mov [ebp - 8], ecx
		mov [ebp - 12], edx
		mov eax, [ebp + 8]
		mov [ebp - 16], eax
		mov eax, [ebp + 12]
		mov [ebp - 20], eax
	}
	__asm {
		jmp __proxy_consume_text
	}
__consume_text_hook:
	__asm {
		mov eax, [ebp - 20]
		push eax
		lea eax, [ebp - 16]
		push eax
		mov eax, [ebp - 12]
		push eax
		mov eax, [ebp - 8]
		push eax
		call ebx
		ret
	}
__proxy_consume_text:
	__asm {
		pushad
		pushfd

		mov ebx, beforeConsumeTextHook
		call __consume_text_hook

		popfd
		popad
	}
	__asm {
		mov eax, [ebp - 20]
		push eax
		mov eax, [ebp - 16]
		push eax
		mov eax, [ebp - 4]
		call eax
		add esp, 8
	}
	__asm {
		pushad
		pushfd

		mov ebx, afterConsumeTextHook
		call __consume_text_hook

		popfd
		popad
	}
	__asm {
		add esp, 20
		pop ebp
		ret
	}
}
static BYTE* origin_vm_ip = NULL;
static CharacterInfo char_info = { 0 };

static void WINAPI beforeConsumeTextHook(RealLiveVMState* sp, RealLiveVMContext* cp, int* byteMode, int a4) {
	origin_vm_ip = NULL;
	if (GetNextCharacterInfo(sp->seenNo, cp->vmIp - cp->vmBase, &char_info)) {
		if (char_info.asciiFlag) {
			cp->vmIp[0] = char_info.character & 0x00FF;
		} else {
			cp->vmIp[0] = char_info.character & 0x00FF;
			cp->vmIp[1] = (BYTE)((char_info.character & 0xFF00) >> 8);
		}
		if (cp->vmIp[0] == '\\') {
			*byteMode = 2;
		} else if (char_info.asciiFlag) {
			*byteMode = 1;
		} else {
			*byteMode = 0;
		}
		origin_vm_ip = cp->vmIp;
	}
}

static void WINAPI afterConsumeTextHook(RealLiveVMState* sp, RealLiveVMContext* cp, int* byteMode, int a4) {
	if (origin_vm_ip == NULL) {
		return;
	}
	BOOL consumeFlag = cp->vmIp != origin_vm_ip;
	if (consumeFlag) {
		AckConsumeCharacter(&char_info);
	}
	cp->vmIp = (consumeFlag && char_info.lastFlag) ? 
		origin_vm_ip + char_info.length : origin_vm_ip;
	RtlZeroMemory(&char_info, sizeof(CharacterInfo));
	origin_vm_ip = NULL;
}

void ASM_FUNCTION HookHandleInstantText() {
	__asm {
		sub esp, 4
		pushfd
		pushad

		mov eax, [esp + 48]
		push eax
		call getTranslatedText
		mov [esp + 36], eax

		popad
		popfd
	}
	__asm {
		pop eax
		mov [esp + 8], eax
		pop eax
		push ebp
		mov ebp, esp
		sub esp, 64
		jmp eax
	}
}

static const char* const WINAPI getTranslatedText(const char* const text) {
	const char* name = GetTranslatedName(text);
	if (name != text) return name;
	return GetTranslatedText(text);
}