#include "asm.h"
#include "define.h"
#include "hook.h"
#include "patch.h"
#include <filesystem>
#include <string>
#include <system_error>
#include <optional>

static void WINAPI handleSeenDataPatch(RealLiveSeenData* ptr, unsigned seenNo) noexcept;
static void WINAPI beforeConsumeTextHook(RealLiveVMState* sp, RealLiveVMContext* cp, int* byteMode, int a4) noexcept;
static void WINAPI afterConsumeTextHook(RealLiveVMState* sp, RealLiveVMContext* cp, int* byteMode, int a4) noexcept;
static BOOL WINAPI isRealLiveWindow(const char* const lpClassName) noexcept;
static std::optional<std::string> ToRelativeToCwd(const std::string& inPath) noexcept;
static const char* const WINAPI applyFileMapping(const char* const text) noexcept;

static void WINAPI checkG00(const char* const file) noexcept;
static BOOL WINAPI isInSeen9900Mode() noexcept;

static BOOL WINAPI loadTranslatedTextInfo(const BYTE* const current_ip) noexcept;
static size_t WINAPI getOriginTextLength() noexcept;
static size_t WINAPI getTranslatedTextLength() noexcept;
static void WINAPI copyTranslatedText(char* const to) noexcept;
static const char* WINAPI getTranslatedName(const char* const name) noexcept;

static BYTE* origin_vm_ip{};
static CharacterInfo char_info{};
static std::optional<std::reference_wrapper<const SeenTextItem>> text_item_ref{};
static DWORD tmp_data{};

C_NAKED_FUNCTION(HookForPatch) {
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

C_NAKED_FUNCTION(HookEnumFontFamiliesExA) {
	__asm {
		pop eax
		push ebx
		mov ebx, [esp + 12]
		mov byte ptr[ebx + 23], 134
		pop ebx
	}
	__asm {
		mov edi, edi
		push ebp
		mov ebp, esp
		jmp eax
	}
}

C_NAKED_FUNCTION(HookCreateFontA) {
	__asm {
		pop eax
		mov dword ptr ss : [esp + 36] , 134
		mov edi, edi
		push ebp
		mov ebp, esp
		jmp eax
	}
}

C_NAKED_FUNCTION(HookCreateFileA) {
	__asm {
		mov eax, [esp + 8]
		push eax
		call applyFileMapping
		test eax, eax
		je __hook_create_file_ret
		mov [esp + 8], eax
	}
__hook_create_file_ret:
	__asm {
		pop eax
		mov edi, edi
		push ebp
		mov ebp, esp
		jmp eax
	}
}

C_NAKED_FUNCTION(HookCreateWindowExA) {
	__asm {
		mov eax, [esp + 12]
		push eax
		call isRealLiveWindow
		test eax, eax
		je __create_window_hook_ret
	}
	__asm {
		pushad
		pushfd
		call PatchHookAfterVMInit
		popfd
		popad
	}
	__asm {
		mov eax, [esp + 4]
		mov tmp_data, eax
		mov eax, HookAfterCreateWindowExA
		mov[esp + 4], eax
	}
__create_window_hook_ret:
	__asm {
		pop eax
		mov edi, edi
		push ebp
		mov ebp, esp
		jmp eax
	}
}

C_NAKED_FUNCTION(HookAfterCreateWindowExA) {
	__asm {
		mov window_handle, eax
		sub esp, 4
		pushfd
		pushad
		mov eax, tmp_data
		mov [esp + 36], eax
		mov tmp_data, 0
	}
	window_proc = reinterpret_cast<WNDPROC>(SetWindowLongA(window_handle, GWLP_WNDPROC, reinterpret_cast<LONG>(ProxyWndProc)));
	__asm {
		popad
		popfd
		ret
	}
}

C_NAKED_FUNCTION(ProxyConsumeTextInQuoteMode) {
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

static void WINAPI beforeConsumeTextHook(RealLiveVMState* sp, RealLiveVMContext* cp, int* byteMode, int a4) noexcept {
	origin_vm_ip = nullptr;
	if (patch_rt->getNextCharacterInfo(sp->seenNo, cp->vmIp - cp->vmBase, char_info)) {
		if (char_info.asciiFlag) {
			cp->vmIp[0] = char_info.character & 0x00FF;
		} else {
			cp->vmIp[0] = char_info.character & 0x00FF;
			cp->vmIp[1] = static_cast<BYTE>((char_info.character & 0xFF00) >> 8);
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

static void WINAPI afterConsumeTextHook(RealLiveVMState* sp, RealLiveVMContext* cp, int* byteMode, int a4) noexcept {
	if (origin_vm_ip == nullptr) {
		return;
	}
	bool consumeFlag = cp->vmIp != origin_vm_ip;
	if (consumeFlag) {
		patch_rt->ackConsumeCharacter(char_info);
	}
	cp->vmIp = (consumeFlag && char_info.lastFlag) ? 
		origin_vm_ip + char_info.length : origin_vm_ip;
	char_info = {};
	origin_vm_ip = nullptr;
}

C_NAKED_FUNCTION(JmpSignature) {
	__asm {
		push esi
		dec esp
		push ebx
		dec ebp
		inc edx
		sub eax, 5F4E4F4EH
		inc ebx
		dec edi
		dec ebp
		dec ebp
		inc ebp
		push edx
		inc ebx
		dec ecx
		inc ecx
		dec esp
		pop edi
		push ebp
		push ebx
		inc ebp
	}
}

C_NAKED_FUNCTION(ProxyOpenG00File) {
	__asm {
		pushad
		pushfd

		push ecx
		call checkG00

		popfd
		popad
	}
	__asm {
		mov eax, image_base
		add eax, OPEN_G00_FILE_FUNC_RVA
		jmp eax
	}
}

C_NAKED_FUNCTION(ProxyRld3dPresent) {
	__asm {
		sub esp, 4
		pushad
		pushfd

		call isInSeen9900Mode
		mov[esp + 36], eax

		popfd
		popad
	}
	__asm {
		pop eax
		test eax, eax
		je __rl_d3d
		mov eax, 1
		ret
	}
__rl_d3d:
	__asm {
		mov eax, image_base
		add eax, RLD3D_PRESENT_IAT_RVA
		mov eax, [eax]
		jmp eax
	}
}

C_NAKED_FUNCTION(HookSelectText) {
	__asm {
		mov eax, image_base
		mov [esp], eax
		push esi
		push edi
		push ebx
		pushad
		pushfd
	}
	__asm {
		inc esi
		push esi
		call loadTranslatedTextInfo
		test eax, eax
		je __hook_select_text_fail
	}
	__asm {
		push edi
		call copyTranslatedText
		call getTranslatedTextLength
		add edi, eax
		mov ebx, eax
		call getOriginTextLength
		add esi, eax
	}
__hook_select_text_success:
	__asm {
		add dword ptr [esp + 48], 0x1265C2
		mov [esp + 36], ebx
		mov [esp + 40], edi
		mov [esp + 44], esi
		jmp __hook_select_text_ret
	}
__hook_select_text_fail:
	__asm {
		add dword ptr [esp + 48], 0x12656A
		jmp __hook_select_text_ret
	}
__hook_select_text_ret:
	__asm {
		popfd
		popad
		pop ebx
		pop edi
		pop esi
		ret
	}
}

C_NAKED_FUNCTION(HookChunkTextLength) {
	__asm {
		mov eax, image_base
		mov [esp], eax
		push edi
		push esi
		push ebx
		pushad
		pushfd
	}
	__asm {
		inc ecx
		mov edi, ecx
		push ecx
		call loadTranslatedTextInfo
		test eax, eax
		je __hook_chunk_text_length_fail
		call getOriginTextLength
		mov ebx, edi
		add ebx, eax
		call getTranslatedTextLength
		mov esi, eax
	}
__hook_chunk_text_length_success:
	__asm {
		add dword ptr [esp + 48], 0xEF265
		mov [esp + 36], ebx
		mov [esp + 40], esi
		mov [esp + 44], edi
		jmp __hook_chunk_text_length_ret
	}
__hook_chunk_text_length_fail:
	__asm {
		add dword ptr [esp + 48], 0xEF21D
		jmp __hook_chunk_text_length_ret
	}
__hook_chunk_text_length_ret:
	__asm {
		popfd
		popad
		pop ebx
		pop esi
		pop edi
		ret
	}
}

C_NAKED_FUNCTION(HookChunkText) {
	__asm {
		mov ebx, image_base
		mov [esp], ebx
		push eax
		push edi
		pushad
		pushfd
	}
	__asm {
		mov esi, edi
		mov edi, eax
		push esi
		call loadTranslatedTextInfo
		test eax, eax
		je __hook_chunk_text_fail
		push edi
		call copyTranslatedText
		call getOriginTextLength
		add esi, eax
		call getTranslatedTextLength
		add edi, eax
	}
__hook_chunk_text_success:
	__asm {
		add dword ptr [esp + 44], 0xEF2DC
		mov dword ptr [esp + 36], esi
		mov dword ptr [esp + 40], edi
		jmp __hook_chunk_text_ret
	}
__hook_chunk_text_fail:
	__asm {
		add dword ptr [esp + 44], 0xEF288
		jmp __hook_chunk_text_ret
	}
__hook_chunk_text_ret:
	__asm {
		popfd
		popad
		pop ebx
		pop eax
		ret
	}
}

C_NAKED_FUNCTION(HookNameText) {
	__asm {
		pushad
		pushfd
	}
	__asm {
		mov eax, [esp + 40]
		push eax
		call getTranslatedName
		mov [esp + 40], eax
	}
	__asm {
		popfd
		popad
		push image_base
		add dword ptr [esp], 0xEBEC0
		ret
	}
}


EXTERN_C LRESULT CALLBACK ProxyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept {
	if (uMsg == WM_PAINT && isInSeen9900Mode()) {
		PAINTSTRUCT ps{};
		HDC hdc = BeginPaint(hWnd, &ps);
		patch_rt->paintCurFrameInSeen9900Mode(hWnd, hdc);
		EndPaint(hWnd, &ps);
		ValidateRect(hWnd, NULL);
	}
	return CallWindowProcA(window_proc, hWnd, uMsg, wParam, lParam);
}

EXTERN_C BOOL WINAPI ProxyInvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase) noexcept {
	return InvalidateRect(hWnd, isInSeen9900Mode() ? NULL : lpRect, bErase);
}

EXTERN_C int WINAPI ProxySetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD w, DWORD h, int xSrc,
	int ySrc, UINT StartScan, UINT cLines, const void* lpvBits, const BITMAPINFO* lpbmi, UINT ColorUse) noexcept {
	if (isInSeen9900Mode()) return 0;
	return SetDIBitsToDevice(hdc, xDest, yDest, w, h, xSrc, ySrc, StartScan, cLines, lpvBits, lpbmi, ColorUse);
}

static void WINAPI handleSeenDataPatch(RealLiveSeenData* ptr, unsigned seenNo) noexcept {
	patch_rt->updateSeenBuffer(ptr->decompressed_data, seenNo);
}

static BOOL WINAPI isRealLiveWindow(const char* const lpClassName) noexcept {
	return strncmp(WINDOW_CLASS_NAME, lpClassName, strlen(WINDOW_CLASS_NAME)) == 0 ? TRUE : FALSE;
}

static const char* const WINAPI applyFileMapping(const char* const text) noexcept {
	char path[MAX_PATH] = { 0 };
	std::optional<std::string> opt = ToRelativeToCwd(text);
	if (!opt) {
		return NULL;
	}
	auto result = patch_rt->applyFilePathMapping(*opt);
	if (!result) {
		return NULL;
	}
	return result->get().c_str();
}

static std::optional<std::string> ToRelativeToCwd(const std::string& inPath) noexcept {
	namespace fs = std::filesystem;
	try {
		std::error_code ec;
		fs::path cwd = fs::current_path(ec);
		if (ec) return std::nullopt;

		fs::path in = fs::path(inPath);
		fs::path absIn = in.is_absolute() ? in : (cwd / in);

		absIn = absIn.lexically_normal();

		fs::path rel = absIn.lexically_relative(cwd);
		if (rel.empty()) return std::nullopt;

		std::string out = rel.string();

		if (out.rfind(".\\", 0) == 0) {
			out.erase(0, 2);
			if (out.empty()) out = ".";
		}
		return out;
	} catch (...) {
		return std::nullopt;
	}
}

static void WINAPI checkG00(const char* const file) noexcept {
	patch_rt->checkG00ForSeen9900(file);
}

static BOOL WINAPI isInSeen9900Mode() noexcept {
	return patch_rt->isInSeen9900Mode() ? TRUE : FALSE;
}

static BOOL WINAPI loadTranslatedTextInfo(const BYTE* const current_ip) noexcept {
	text_item_ref = std::nullopt;
	int offset = current_ip - context_pointer->vmBase;
	if (offset <= 0) return FALSE;
	auto opt = patch_rt->getTranslatedText(state_pointer->seenNo, offset);
	if (!opt) return FALSE;
	text_item_ref = opt;
	return TRUE;
}

static size_t WINAPI getOriginTextLength() noexcept {
	return text_item_ref->get().length;
}

static size_t WINAPI getTranslatedTextLength() noexcept {
	return text_item_ref->get().translated.length();
}

static void WINAPI copyTranslatedText(char* const to) noexcept {
	const std::string& text = text_item_ref->get().translated;
	memcpy(to, text.c_str(), text.length());
}

static const char* WINAPI getTranslatedName(const char* const name) noexcept {
	auto opt = patch_rt->getTranslatedName(name);
	return opt ? opt->get().c_str() : name;
}