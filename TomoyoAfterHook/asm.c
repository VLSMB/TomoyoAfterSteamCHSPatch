#include "asm.h"

void WINAPI handleSeenDataPatch(SeenData* ptr, int num);
void WINAPI beforeConsumeTextHook(void* p1, void* p2, int* byteMode, int a4);
void WINAPI afterConsumeTextHook(void* p1, void* p2, int* byteMode, int a4);
int WINAPI checkFileIsSeen(const char* fileName);

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
		mov [edx + 28], eax
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

void WINAPI handleSeenDataPatch(SeenData* ptr, int num) {
	BYTE* data = GetSeenPatchData(num);
	if (data != NULL && ptr->decompressed_data != NULL && ptr->decompressed_size > 0) {
		memcpy(ptr->decompressed_data, data, ptr->decompressed_size);
	}
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
		je __create_file_ret
	}
	__asm {
		pushfd
		pushad
		call PatchHookAfterOpenSeenFile
		popad
		popfd
	}
__create_file_ret:
	__asm {
		pop eax
		mov edi, edi
		push ebp
		mov ebp, esp
		jmp eax
	}
}

int WINAPI checkFileIsSeen(const char* fileName) {
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

const char* const text = "神户小鸟天下第一！VLSMB1英文字符测试。";
const char* p = NULL;
BYTE* origin_ip = NULL;

void WINAPI beforeConsumeTextHook(void* p1, void* p2, int* byteMode, int a4) {
	BYTE* buffer = GET_VM_IP_POINTER(p2);
	if (*buffer == 0xC4 && *(buffer + 1) == 0xE3) {
		if (p == NULL) {
			p = text;
		}
		if (*p == '\\') {
			*byteMode = 2;
		} else if ((BYTE)(*p) < 0x80) {
			*byteMode = 1;
		} else {
			*byteMode = 0;
		}
		origin_ip = buffer;
		SET_VM_IP_POINTER(p2, p);
	}
}

void WINAPI afterConsumeTextHook(void* p1, void* p2, int* byteMode, int a4) {
	if (origin_ip == NULL) {
		return;
	}
	BYTE* buffer = GET_VM_IP_POINTER(p2);
	if (*buffer) {
		SET_VM_IP_POINTER(p2, origin_ip);
		p = buffer;
	}
	else {
		SET_VM_IP_POINTER(p2, origin_ip + 2);
		p = NULL;
	}
	origin_ip = NULL;
}

const char* kotori = "神户小鸟";

void ASM_FUNCTION HookHandleNameText() {
	__asm {
		mov edx, kotori
		pop eax

		push ebp
		mov ebp, esp
		sub esp, 64
		jmp eax
	}
}