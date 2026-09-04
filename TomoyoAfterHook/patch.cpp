#include "patch.h"
#include <type_traits>
#include <unordered_map>
#include <map>
#include <stack>
#include <new>
#include <regex>

template<typename T>
using seen_map = std::unordered_map<unsigned, std::unordered_map<unsigned, T>>;

static std::unordered_map<std::string, std::string> name_map;
static size_t max_name_length = 0;
static seen_map<std::string> text_map;
static seen_map<unsigned> text_length_map;
static seen_map<std::stack<BYTE>> text_stack;
static std::unordered_map<std::string, std::string> short_text_map;
static std::unordered_map<unsigned, std::vector<unsigned>> seen_offset_map;
static std::unordered_map<std::string, std::string> cached_text_map;
static size_t max_cached_text_length = 0;
static std::string current_title_text = "";

static const char* const pureGetTranslatedText(const char* const text);

EXTERN_C BOOL InitPatchData(PatchPack* in) {
	if (in == NULL) return FALSE;
	if (in->pName->pointer != NULL && in->pName->size > 0) {
		name_map.reserve(in->pName->size);
		for (size_t i = 0; i < in->pName->size; i++) {
			NameData& e = in->pName->pointer[i];
			name_map.insert(std::make_pair(
				bufferToStdString(e.origin), bufferToStdString(e.translated)
			));
			if (e.origin.size > max_name_length) {
				max_name_length = e.origin.size;
			}
		}
	}
	if (in->pSize > 0 && in->pText != NULL) {
		text_map.reserve(in->pSize);
		text_stack.reserve(in->pSize);
		text_length_map.reserve(in->pSize);
		seen_offset_map.reserve(in->pSize);
		for (size_t i = 0; i < in->pSize; i++) {
			SeenPatchDataArray* pArray = in->pText[i];
			auto& map = text_map[pArray->seenNo];
			map.reserve(pArray->size);
			text_stack[pArray->seenNo].reserve(pArray->size);
			text_length_map[pArray->seenNo].reserve(pArray->size);
			for (size_t j = 0; j < pArray->size; j++) {
				map[pArray->pointer[j].offset] = bufferToStdString(pArray->pointer[j].translated);
				text_length_map[pArray->seenNo][pArray->pointer[j].offset] = pArray->pointer[j].length;
				if (pArray->pointer[j].length <= SHOT_TEXT_SIZE) {
					short_text_map.insert(std::make_pair(
						bufferToStdString(pArray->pointer[j].origin),
						bufferToStdString(pArray->pointer[j].translated)
					));
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
	short_text_map.clear();
	seen_offset_map.clear();
	cached_text_map.clear();
}

EXTERN_C const char* const GetTranslatedName(const char* const name) {
	size_t count = 0;
	while (name[count] && count <= max_name_length) count++;
	if (count > max_name_length) return name;
	auto it = name_map.find(std::string(name));
	return it == name_map.end() ? name : it->second.c_str();
}

EXTERN_C const char* const GetTranslatedText(const char* const text) {
	const char* translated = pureGetTranslatedText(text);
	if (translated != text) return translated;
	size_t count = 0;
	const char* p = text;
	while (p[count] && count <= max_cached_text_length) count++;
	if (count <= max_cached_text_length) {
		const auto& it = cached_text_map.find(std::string(text));
		if (it != cached_text_map.end()) {
			return it->second.c_str();
		}
	}
	p = text;
	while (*p && (isdigit(*p) || isspace(*p))) p++;
	translated = pureGetTranslatedText(p);
	if (translated == p) {
		return text;
	}
	std::string str;
	const char* q = text;
	while (q < p) {
		str += *q;
		q++;
	}
	str += translated;
	if (max_cached_text_length < str.size()) {
		max_cached_text_length = str.size();
	}
	cached_text_map[std::string(text)] = str;
	return cached_text_map[std::string(text)].c_str();
}

EXTERN_C void UpdateSeenBuffer(BYTE* buffer, unsigned seenNo) {
	std::vector<BYTE> bytes;
	for (const auto& offset : seen_offset_map[seenNo]) {
		DWORD dword = seenOffsetToDWORD(seenNo, offset);
		BYTE* p = buffer + offset;
		bytes.push_back(TEXT_META_MAGIC);
		bytes.push_back(dword & 0xFF);
		bytes.push_back((dword >> 8) & 0xFF);
		bytes.push_back((dword >> 16) & 0xFF);
		bytes.push_back((dword >> 24) & 0xFF);
		std::string s = vectorToHex(bytes);
		for (const char& c : s) {
			*p = static_cast<BYTE>(c);
			p++;
		}
		bytes.clear();
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
	current_title_text = std::string(WINDOW_TITLE"  ") + pureGetTranslatedText(subTitle.c_str());
	if (!debugSuf.empty()) {
		current_title_text += " " + debugSuf;
	}
	return current_title_text.c_str();
}

static const char* const pureGetTranslatedText(const char* const text) {
	unsigned seenNo = 0;
	unsigned offset = 0;
	if (exactSeenOffset(text, seenNo, offset)) {
		const auto& map = text_map.find(seenNo);
		if (map == text_map.end()) {
			return text;
		}
		const auto& it = map->second.find(offset);
		if (it == map->second.end()) {
			return text;
		}
		return it->second.c_str();
	}
	bool isShortText = false;
	for (int i = 0; i <= SHOT_TEXT_SIZE && !isShortText; i++) {
		isShortText = text[i] == '\0';
	}
	if (!isShortText) {
		return text;
	}
	const auto& it = short_text_map.find(std::string(text));
	return it == short_text_map.end() ? text : it->second.c_str();
}