#include "patch.h"
#include "utils.h"
#include <type_traits>
#include <unordered_map>
#include <map>
#include <stack>
#include <new>
#include <regex>

namespace Patch {
	class Seen9900Flag {
	private:
		bool typeEnFlag;
		bool type999Flag;
		int index;
		std::string text;
		unsigned frameCount;
		unsigned curFrame;
		const std::vector<std::string>& seen9900_vector;

		void loadTextAndFrame() {
			resetFrame();
			if (!isFlag()) return;
			text = seen9900_vector[index];
			curFrame = 0;
			frameCount = 0;
		}
		void resetFrame() {
			text = "";
			frameCount = 0;
			curFrame = 0;
		}

	public:
		Seen9900Flag(const std::vector<std::string>& v) : seen9900_vector(v) {
			reset();
		}
		void reset() {
			typeEnFlag = false;
			type999Flag = false;
			index = -1;
			resetFrame();
		}
		void setIndex(size_t idx) {
			reset();
			if (idx < seen9900_vector.size()) {
				index = idx;
			}
		}
		void setTypeEn() {
			if (index == -1) {
				reset();
			} else {
				typeEnFlag = true;
				type999Flag = false;
				resetFrame();
			}
		}
		void setType999() {
			if (index == -1 || !typeEnFlag) {
				reset();
			} else {
				type999Flag = true;
				loadTextAndFrame();
			}
		}
		bool isFlag() const {
			return index >= 0 && typeEnFlag && type999Flag;
		}
		unsigned currentFrame() {
			return frameCount++;
			//return curFrame < frameCount ? (curFrame++) : curFrame;
		}
		std::string curText() const {
			return text;
		}
	};
};

template<typename T>
using seen_map = std::unordered_map<unsigned, std::unordered_map<unsigned, T>>;

static std::unordered_map<std::string, std::string> name_map;
static size_t max_name_length = 0;
static seen_map<std::string> text_map;
static seen_map<unsigned> text_length_map;
static seen_map<std::stack<BYTE>> text_stack;
static std::unordered_map<unsigned, std::vector<unsigned>> replace_flag_map;
static std::unordered_map<unsigned, std::vector<unsigned>> seen_offset_map;

static std::unordered_map<DWORD, std::string> pe_resource_str_map;
static std::unordered_map<DWORD, DWORD> pe_resource_length_map;
static std::unordered_map<std::string, std::string> file_mapping;
static std::string current_title_text = "";
static std::vector<std::string> seen9900_vector;
static Patch::Seen9900Flag seen9900_flag(seen9900_vector);

EXTERN_C BOOL InitPatchData(PatchPack* in) {
	if (in == NULL) return FALSE;

	if (in->pName != NULL && in->pName->pointer != NULL && in->pName->size > 0) {
		name_map.reserve(in->pName->size);
		for (size_t i = 0; i < in->pName->size; i++) {
			const NameData& e = in->pName->pointer[i];
			name_map.insert(std::make_pair(
				bufferToStdString(e.origin), bufferToStdString(e.translated)
			));
			if (e.origin.size > max_name_length) {
				max_name_length = e.origin.size;
			}
		}
	}

	if (in->pRes != NULL && in->pRes->pointer != NULL && in->pRes->size > 0) {
		pe_resource_str_map.reserve(in->pRes->size);
		pe_resource_length_map.reserve(in->pRes->size);
		for (size_t i = 0; i < in->pRes->size; i++) {
			const PEResource& res = in->pRes->pointer[i];
			std::string str = bufferToStdString(res.translated);
			if (res.encode == UTF16LE) {
				std::string newStr;
				if (!GbkToUtf16Le(str, newStr)) {
					continue;
				}
				str = newStr;
			}
			if (str.length() > res.length) {
				continue;
			}
			if (res.strategy == FILL_SPACE) {
				if (res.encode == GBK) {
					str += std::string(res.length - str.length(), ' ');
				} else if (res.encode == UTF16LE) {
					std::string space;
					GbkToUtf16Le(std::string((res.length - str.length()) / 2, ' '), space);
					str += space;
				}
			}
			pe_resource_str_map.insert(std::make_pair(
				res.rva, str
			));
			pe_resource_length_map.insert(std::make_pair(
				res.rva, res.length
			));
		}
	}

	if (in->pFileMap != NULL && in->pFileMap->pointer != NULL && in->pFileMap->size > 0) {
		file_mapping.reserve(in->pFileMap->size);
		for (size_t i = 0; i < in->pFileMap->size; i++) {
			const FileMapping& e = in->pFileMap->pointer[i];
			std::string origin = bufferToStdString(e.origin);
			std::transform(origin.begin(), origin.end(), origin.begin(),
				[](unsigned char c) { return std::toupper(c); });
			file_mapping.insert(std::make_pair(
				origin, bufferToStdString(e.target)
			));
		}
	}

	if (in->pSize > 0 && in->pText != NULL) {
		text_map.reserve(in->pSize);
		text_stack.reserve(in->pSize);
		text_length_map.reserve(in->pSize);
		seen_offset_map.reserve(in->pSize);
		replace_flag_map.reserve(in->pSize);
		for (size_t i = 0; i < in->pSize; i++) {
			SeenPatchDataArray* pArray = in->pText[i];
			if (pArray->seenNo == SEEN_9900) {
				seen9900_vector.reserve(pArray->size);
				seen_offset_map[pArray->seenNo].reserve(pArray->size);
				for (size_t j = 0; j < pArray->size; j++) {
					seen9900_vector.push_back(
						bufferToStdString(pArray->pointer[j].translated)
					);
					seen_offset_map[pArray->seenNo].push_back(pArray->pointer[j].offset);
				}
				continue;
			}
			text_map[pArray->seenNo].reserve(pArray->size);
			text_stack[pArray->seenNo].reserve(pArray->size);
			text_length_map[pArray->seenNo].reserve(pArray->size);
			replace_flag_map[pArray->seenNo].reserve(pArray->size);
			for (size_t j = 0; j < pArray->size; j++) {
				text_map[pArray->seenNo][pArray->pointer[j].offset] = bufferToStdString(pArray->pointer[j].translated);
				text_length_map[pArray->seenNo][pArray->pointer[j].offset] = pArray->pointer[j].length;
				if (pArray->pointer[j].replaceFlag) {
					if (pArray->pointer[j].translated.size > pArray->pointer[j].length) {
						return FALSE;
					}
					replace_flag_map[pArray->seenNo].push_back(pArray->pointer[j].offset);
				} else {
					seen_offset_map[pArray->seenNo].push_back(pArray->pointer[j].offset);
				}
			}
		}
	}
	return TRUE;
}

EXTERN_C void CleanPatchData() {
	name_map.clear();
	text_map.clear();
	text_stack.clear();
	text_length_map.clear();
	seen_offset_map.clear();
	replace_flag_map.clear();
	pe_resource_length_map.clear();
	pe_resource_str_map.clear();
	file_mapping.clear();
	seen9900_vector.clear();
	seen9900_flag.reset();
}

EXTERN_C const char* const GetTranslatedName(const char* const name) {
	size_t count = 0;
	while (name[count] && count <= max_name_length) count++;
	if (count > max_name_length) return name;
	auto it = name_map.find(std::string(name));
	return it == name_map.end() ? name : it->second.c_str();
}

EXTERN_C BOOL GetTranslatedText(TextInfo* info) {
	if (info == NULL) return FALSE;
	const auto& textMap = text_map.find(info->seenNo);
	if (textMap == text_map.end()) {
		return FALSE;
	}
	const auto& textIt = textMap->second.find(info->offset);
	if (textIt == textMap->second.end()) {
		return FALSE;
	}
	const auto& lenMap = text_length_map.find(info->seenNo);
	if (lenMap == text_length_map.end()) {
		return FALSE;
	}
	const auto& lenIt = lenMap->second.find(info->offset);
	if (lenIt == lenMap->second.end()) {
		return FALSE;
	}
	info->origin_length = lenIt->second;
	info->translated_length = textIt->second.length();
	info->translated_text = textIt->second.c_str();
	return TRUE;
}

EXTERN_C void SetTranslatedPEResource(void* imageBase) {
	for (const auto& p : pe_resource_str_map) {
		void* addr = (void*)((DWORD)imageBase + p.first);
		size_t len = pe_resource_length_map[p.first];
		BYTE* buffer = new BYTE[len];
		RtlZeroMemory(buffer, sizeof(BYTE) * len);
		memcpy_s(buffer, len, p.second.c_str(), p.second.length());
		DWORD oldProtect = 0;
		VirtualProtect(addr, len, PAGE_READWRITE, &oldProtect);
		memcpy_s(addr, len, buffer, len);
		DWORD tmp = 0;
		VirtualProtect(addr, len, oldProtect, &tmp);
	}
}

EXTERN_C void UpdateSeenBuffer(BYTE* buffer, unsigned seenNo) {
	if (seenNo == SEEN_9900) {
		if (seen_offset_map[seenNo].size() > 256) {
			return;
		}
		for (size_t i = 0; i < seen_offset_map[seenNo].size(); ++i) {
			if (seen9900_vector[i].length() < 3) {
				continue;
			}
			std::string s = "&" + vectorToHex({ (BYTE)i });
			BYTE* p = buffer + seen_offset_map[seenNo][i];
			for (const char& c : s) {
				*p = static_cast<BYTE>(c);
				p++;
			}
		}
		return;
	}
	for (const auto& offset : replace_flag_map[seenNo]) {
		const std::string& text = text_map[seenNo][offset];
		if (text == "") continue;
		memcpy(buffer + offset, text.c_str(), text.length());
	}
	if (seenNo == SEEN_PATCH_NO) {
		BYTE* p = buffer + SEEN_PATCH_OFFSET;
		for (size_t i = 0; i < SEEN_PATCH_LEN; i++) {
			p[i] = SEEN_PATCH_DATA[i];
		}
	}
}

EXTERN_C BOOL GetNextCharacterInfo(unsigned seenNo, unsigned offset, CharacterInfo* out) {
	const auto& textMap = text_map.find(seenNo);
	if (textMap == text_map.end()) {
		return FALSE;
	}
	const auto& it = textMap->second.find(offset);
	if (it == textMap->second.end()) {
		return FALSE;
	}
	const std::string& buffer = it->second;

	RtlZeroMemory(out, sizeof(CharacterInfo));
	out->seenNo = seenNo;
	out->offset = offset;
	out->length = text_length_map[seenNo][offset];

	const auto& stackMap = text_stack.find(seenNo);
	bool inStack = stackMap != text_stack.end() &&
		stackMap->second.find(offset) != stackMap->second.end();
	if (inStack) {
		const auto& it = stackMap->second.find(offset);
		std::stack<BYTE>& stack = it->second;
		if (stack.empty()) {
			stackMap->second.erase(it);
			return FALSE;
		}
		BYTE bl = stack.top();
		stack.pop();
		if ((bl >= 0x80 || bl == '\\') && !stack.empty()) {
			WORD bh = stack.top();
			stack.pop();
			out->asciiFlag = 0;
			out->character = ((bh << 8) & 0xFF00) | (bl & 0x00FF);
		} else {
			out->asciiFlag = 1;
			out->character = bl & 0x00FF;
		}
		out->lastFlag = stack.empty() ? 1 : 0;
		if (out->asciiFlag) {
			stack.push(out->character);
		} else {
			stack.push((BYTE)(out->character & 0xFF00) >> 8);
			stack.push((BYTE)(out->character & 0x00FF));
		}
		return TRUE;
	}

	if (buffer.size() == 0) {
		return FALSE;
	} else if (buffer.size() == 1) {
		out->asciiFlag = 1;
		out->lastFlag = 1;
		out->character = static_cast<BYTE>(buffer[0]) & 0x00FF;
	} else {
		out->asciiFlag = (static_cast<BYTE>(buffer[0]) >= 0x80 || static_cast<BYTE>(buffer[0]) == '\\') ? 0 : 1;
		out->lastFlag = (!out->asciiFlag && buffer.size() == 2) ? 1 : 0;
		if (out->asciiFlag) {
			out->character = static_cast<BYTE>(buffer[0]);
		} else {
			out->character = static_cast<BYTE>(buffer[1]);
			out->character = ((out->character << 8) & 0xFF00) | (static_cast<BYTE>(buffer[0]) & 0x00FF);
		}
	}
	std::stack<BYTE>& stack = text_stack[seenNo][offset];
	for (auto it = buffer.rbegin(); it != buffer.rend(); ++it) {
		stack.push(static_cast<BYTE>(*it));
	}
	return TRUE;
}

EXTERN_C void AckConsumeCharacter(const CharacterInfo* out) {
	std::stack<BYTE>& stack = text_stack[out->seenNo][out->offset];
	if (!stack.empty()) stack.pop();
	if (!out->asciiFlag && !stack.empty()) stack.pop();
	if (stack.empty()) {
		text_stack[out->seenNo].erase(text_stack[out->seenNo].find(out->offset));
	}
}

EXTERN_C BOOL IsWindowTitleText(const char* const text) {
	return startsWith(text, ORIGIN_TITLE_NAME) ? TRUE : FALSE;
}

EXTERN_C const char* const GetWindowTitleTranslatedText(const char* const text) {
	if (!IsWindowTitleText(text)) return text;
	std::string subTitle, debugSuf;
	extractRlDebugSuffix(removePrefix(text, ORIGIN_TITLE_NAME), subTitle, debugSuf);
	current_title_text = std::string(WINDOW_TITLE"  ") + GetTranslatedName(subTitle.c_str());
	if (!debugSuf.empty()) {
		current_title_text += " " + debugSuf;
	}
	return current_title_text.c_str();
}

EXTERN_C const char* const ApplyFilePathMapping(const char* const text) {
	std::string s(text);
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return std::toupper(c); });
	const auto& p = file_mapping.find(s);
	return p != file_mapping.end() ? p->second.c_str() : text;
}

EXTERN_C const char* const CheckTextForSeen9900(const char* const text) {
	if (std::string(text, strnlen(text, 8)) == "S_TYPING") {
		return text;
	}
	std::string s(text, strnlen(text, 3));
	std::vector<BYTE> vec;
	if (s.length() == 3 && s[0] == '&'
		&& hexToVector(vec, s.substr(1)) && vec.size() == 1) {
		size_t idx = (size_t)vec[0];
		seen9900_flag.setIndex(idx);
		memset(TRASH_BUFFER, 'A', sizeof(TRASH_BUFFER));
		TRASH_BUFFER[seen9900_vector[idx].length()] = '\0';
		return TRASH_BUFFER;
	} else {
		seen9900_flag.reset();
		return text;
	}
}

EXTERN_C void CheckG00ForSeen9900(const char* const file) {
	std::string s(file, strnlen(file, 15));
	if (s == "S_TYPING999.g00") {
		seen9900_flag.setType999();
	} else if (s == "S_TYPING_EN.g00") {
		seen9900_flag.setTypeEn();
	} else {
		seen9900_flag.reset();
	}
}

EXTERN_C BOOL IsInSeen9900Mode() {
	return seen9900_flag.isFlag() ? TRUE : FALSE;
}

EXTERN_C void PaintCurFrameInSeen9900Mode(HWND hwnd, HDC hdc) {
	std::string text = seen9900_flag.curText();
	if (text.empty()) return;

	RECT rc;
	GetClientRect(hwnd, &rc);
	FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	int fontSize = (rc.bottom - rc.top) / 25;
	if (fontSize < 12) fontSize = 12;
	if (fontSize > 96) fontSize = 96;

	HFONT hFont = CreateFontA(
		-fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, SEEN_9900_MODE_FONT);
	HGDIOBJ oldFont = SelectObject(hdc, hFont);

	int oldMode = SetBkMode(hdc, TRANSPARENT);
	COLORREF oldColor = SetTextColor(hdc, RGB(0, 0, 0));

	DrawTextA(hdc, text.c_str(), (int)text.length(), &rc,
		DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	SetTextColor(hdc, oldColor);
	SetBkMode(hdc, oldMode);
	SelectObject(hdc, oldFont);
	DeleteObject(hFont);
}
