#include "asm.h"

void ASM_FUNCTION HookForDump() {
	__asm {
		push offset hook
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
	hook:
		pushad
		pushfd
	}
	runDump();
	__asm {
		popfd
		popad
		ret
	}
}