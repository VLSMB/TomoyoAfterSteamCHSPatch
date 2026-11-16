#include "patch.h"
#include "define.h"
#include <algorithm>

static bool GbkToUtf16Le(const std::string& gbk, std::string& out);

static Seen9900Flag seen9900_flag{};

void Seen9900Flag::reset() {
	this->typeEnFlag = false;
	this->type999Flag = false;
	this->offset = 0;
}

void Seen9900Flag::setOffset(unsigned off) {
	this->reset();
	this->offset = off;
}

void Seen9900Flag::setTypeEn() {
	if (this->offset == 0) {
		this->reset();
	} else {
		this->typeEnFlag = true;
		this->type999Flag = false;
	}
}

void Seen9900Flag::setType999() {
	if (this->offset == 0 || !this->typeEnFlag) {
		this->reset();
	} else {
		this->type999Flag = true;
	}
}

bool Seen9900Flag::isFlag() const {
	return this->offset > 0 && this->typeEnFlag && this->type999Flag;
}

unsigned Seen9900Flag::getOffset() const {
	return this->offset;
}

SeenTextRT::SeenTextRT(SeenTextItem&& item) : SeenTextItem(std::move(item)) {

}

PatchRT::PatchRT(PatchPack&& pack) {
	this->name_map.reserve(pack.name.data.size());
	for (NameItem& item : pack.name.data) {
		std::string key = item.origin;
		this->name_map.try_emplace(std::move(key), std::move(item));
	}
	this->pe_res.reserve(pack.res.data.size());
	for (PEResourceItem& item : pack.res.data) {
		this->pe_res.push_back(std::move(item));
	}
	this->file_mapping.reserve(pack.fileMap.data.size());
	for (FileMappingItem& item : pack.fileMap.data) {
		std::transform(item.origin.begin(), item.origin.end(), item.origin.begin(),
			[](unsigned char c) { return std::toupper(c); });
		std::string key = item.origin;
		this->file_mapping.try_emplace(std::move(key), std::move(item));
	}

	this->seen_map.reserve(pack.text.size());
	for (const SeenTextData& text : pack.text) {
		this->seen_map[text.seenNo].reserve(text.items.size());
	}
	for (SeenTextData& text : pack.text) {
		std::unordered_map<unsigned, SeenTextRT>& map = this->seen_map[text.seenNo];
		for (SeenTextItem& item : text.items) {
			unsigned offset = item.offset;
			map.try_emplace(offset, SeenTextRT(std::move(item)));
		}
	}
}

std::optional<std::reference_wrapper<const std::string>>
	PatchRT::getTranslatedName(const std::string& name) const {
	auto p = this->name_map.find(name);
	if (p == this->name_map.end()) return std::nullopt;
	return std::cref(p->second.translated);
}

std::optional<std::reference_wrapper<const SeenTextItem>>
	PatchRT::getTranslatedText(unsigned seenNo, unsigned offset) const {
	auto p1 = this->seen_map.find(seenNo);
	if (p1 == this->seen_map.end()) return std::nullopt;
	auto p2 = p1->second.find(offset);
	if (p2 == p1->second.end()) return std::nullopt;
	if (seenNo == SEEN_9900) {
		seen9900_flag.setOffset(offset);
	}
	return std::cref(p2->second);
}

std::optional<std::reference_wrapper<const std::string>>
	PatchRT::applyFilePathMapping(const std::string& path) const {
	std::string pathUpper(path);
	std::transform(pathUpper.begin(), pathUpper.end(), pathUpper.begin(),
		[](unsigned char c) { return std::toupper(c); });
	auto p = this->file_mapping.find(pathUpper);
	if (p == this->file_mapping.end()) return std::nullopt;
	return std::cref(p->second.target);
}

void PatchRT::setTranslatedPERes(void* imageBase) const {
	auto addr = [imageBase](DWORD rva) {
			return reinterpret_cast<void*>(reinterpret_cast<DWORD>(imageBase) + rva);
		};
	for (const PEResourceItem& res : this->pe_res) {
		std::string text = res.translated;
		if (res.encode == PEResourceEncode::UTF16LE) {
			std::string newStr;
			if (!GbkToUtf16Le(text, newStr)) continue;
			text = newStr;
		}
		if (text.length() > res.length) continue;
		if (res.strategy == PEResourceStrategy::FILL_SPACE) {
			if (res.encode == PEResourceEncode::GBK) {
				text += std::string(res.length - text.length(), ' ');
			} else if (res.encode == PEResourceEncode::UTF16LE) {
				std::string space;
				GbkToUtf16Le(std::string((res.length - text.length()) / 2, ' '), space);
				text += space;
			}
		} else if (res.strategy == PEResourceStrategy::FILL_NULL) {
			text += std::string(res.length - text.length(), '\0');
		}
		void* p = addr(res.rva);
		DWORD oldProtect = 0;
		VirtualProtect(p, res.length, PAGE_READWRITE, &oldProtect);
		memcpy_s(p, res.length, text.c_str(), text.length());
		DWORD tmp = 0;
		VirtualProtect(p, res.length, oldProtect, &tmp);
	}
}

void PatchRT::updateSeenBuffer(BYTE* buffer, unsigned seenNo) const {
	if (seenNo == SEEN_PATCH_NO) {
		BYTE* p = buffer + SEEN_PATCH_OFFSET;
		for (size_t i = 0; i < SEEN_PATCH_LEN; i++) {
			p[i] = SEEN_PATCH_DATA[i];
		}
	}
	/*auto map = this->seen_map.find(seenNo);
	if (map == this->seen_map.end()) return;
	for (const auto& [offset, item] : map->second) {
		if (!item.replaceFlag || item.translated == "" 
			|| item.translated.length() > item.length) continue;
		memcpy(buffer + offset, item.translated.c_str(), item.translated.length());
	}*/
}

bool PatchRT::getNextCharacterInfo(unsigned seenNo, unsigned offset, CharacterInfo& info) {
	const auto& textMap = this->seen_map.find(seenNo);
	if (textMap == this->seen_map.end()) {
		return false;
	}
	const auto& it = textMap->second.find(offset);
	if (it == textMap->second.end()) {
		return false;
	}
	SeenTextRT& item = it->second;

	info = {};
	info.seenNo = seenNo;
	info.offset = offset;
	info.length = item.length;
	if (item.inStack) {
		std::stack<BYTE>& stack = item.text_stack;
		if (stack.empty()) {
			item.inStack = false;
			return false;
		}
		BYTE bl = stack.top();
		stack.pop();
		if ((bl >= 0x80 || bl == '\\') && !stack.empty()) {
			WORD bh = stack.top();
			stack.pop();
			info.asciiFlag = false;
			info.character = ((bh << 8) & 0xFF00) | (bl & 0x00FF);
		} else {
			info.asciiFlag = true;
			info.character = bl & 0x00FF;
		}
		info.lastFlag = stack.empty() ? 1 : 0;
		if (info.asciiFlag) {
			stack.push(static_cast<BYTE>(info.character & 0x00FF));
		} else {
			stack.push(static_cast<BYTE>((info.character & 0xFF00) >> 8));
			stack.push(static_cast<BYTE>((info.character & 0x00FF)));
		}
		return true;
	}

	if (item.translated.length() == 0) {
		return false;
	} else if (item.translated.length() == 1) {
		info.asciiFlag = true;
		info.lastFlag = true;
		info.character = static_cast<BYTE>(item.translated[0]) & 0x00FF;
	} else {
		info.asciiFlag = !(static_cast<BYTE>(item.translated[0]) >= 0x80
			|| static_cast<BYTE>(item.translated[0]) == '\\');
		info.lastFlag = (!info.asciiFlag && item.translated.length() == 2);
		if (info.asciiFlag) {
			info.character = static_cast<BYTE>(item.translated[0]);
		} else {
			info.character = static_cast<BYTE>(item.translated[1]);
			info.character = ((info.character << 8) & 0xFF00) | (static_cast<BYTE>(item.translated[0]) & 0x00FF);
		}
	}
	std::stack<BYTE>& stack = item.text_stack;
	while (!stack.empty()) stack.pop();
	item.inStack = true;
	for (auto it = item.translated.rbegin(); it != item.translated.rend(); ++it) {
		stack.push(static_cast<BYTE>(*it));
	}
	return true;
}

void PatchRT::ackConsumeCharacter(const CharacterInfo& info) {
	const auto& map = this->seen_map.find(info.seenNo);
	if (map == this->seen_map.end()) return;
	const auto& it = map->second.find(info.offset);
	if (it == map->second.end()) return;
	std::stack<BYTE>& stack = it->second.text_stack;
	if (!stack.empty()) stack.pop();
	if (!info.asciiFlag && !stack.empty()) stack.pop();
	if (stack.empty()) {
		it->second.inStack = false;
	}
}

void PatchRT::checkG00ForSeen9900(const std::string& file) {
	if (file == "S_TYPING999.g00") {
		seen9900_flag.setType999();
	} else if (file == "S_TYPING_EN.g00") {
		seen9900_flag.setTypeEn();
	} else {
		seen9900_flag.reset();
	}
}

bool PatchRT::isInSeen9900Mode() const {
	return seen9900_flag.isFlag();
}

void PatchRT::paintCurFrameInSeen9900Mode(HWND hwnd, HDC hdc) {
	if (!isInSeen9900Mode()) return;
	unsigned offset = seen9900_flag.getOffset();
	const std::string& text = this->seen_map[SEEN_9900][offset].translated;
	if (text == "") return;

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

static bool GbkToUtf16Le(const std::string& gbk, std::string& out) {
	out.clear();
	if (gbk.empty()) return true;

	int wlen = MultiByteToWideChar(936, 0, gbk.data(), (int)gbk.size(), nullptr, 0);
	if (wlen <= 0) return false;

	std::wstring wstr(static_cast<size_t>(wlen), L'\0');
	if (MultiByteToWideChar(936, 0, gbk.data(), (int)gbk.size(),
		&wstr[0], wlen) <= 0)
		return false;

	out.resize(wstr.size() * sizeof(wchar_t));
	memcpy((void*)out.data(), wstr.data(), out.size());
	return true;
}
