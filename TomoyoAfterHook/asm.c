#include "asm.h"

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
		push ebp
		mov ebp, esp
		and esp, 0xFFFFFFF8
	}
}