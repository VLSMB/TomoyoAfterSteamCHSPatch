#include "asm.h"

void WINAPI handleSeenDataPatch(SeenData* ptr, int num);

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