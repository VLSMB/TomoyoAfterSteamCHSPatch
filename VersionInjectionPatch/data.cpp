#include "data.h"
#include "utils.h"
#include <set>
#include <unordered_map>

void SeenTextData::loadFromBinFile(std::vector<BYTE>::const_iterator begin,
	std::vector<BYTE>::const_iterator end) {
	DWORD magic1 = 0, magic2 = 0, seenNo = 0, count = 0;
	if (begin != end) {
		if (!readDWORD(begin, end, magic1) || !readDWORD(begin, end, magic2) ||
			!readDWORD(begin, end, seenNo) || !readDWORD(begin, end, count) ||
			magic1 != BIN_MAGIC || magic2 != TEXT_BIN_MAGIC) {
			count = 0;
		}
		this->seenNo = seenNo;
		for (size_t i = 0; i < count; ++i) {
			DWORD replaceFlag, off, length, ts;
			if (!readDWORD(begin, end, replaceFlag) || !readDWORD(begin, end, off)
				|| !readDWORD(begin, end, length) || !readDWORD(begin, end, ts)) break;

			SeenTextItem d;
			d.replaceFlag = replaceFlag ? true : false;
			d.index = i;
			d.offset = off;
			d.length = length;
			if (!readString(begin, end, ts, d.translated))break;
			this->items.push_back(std::move(d));
		}
	}
}

std::vector<BYTE> SeenTextData::saveAsBinFile() const {
	std::vector<BYTE> b;
	size_t n = this->items.size();
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, TEXT_BIN_MAGIC);
	appendDWORD(b, this->seenNo);
	appendDWORD(b, n);
	for (const SeenTextItem& d : this->items) {
		appendDWORD(b, d.replaceFlag ? 1 : 0);
		appendDWORD(b, d.offset);
		appendDWORD(b, d.length);
		appendDWORD(b, d.translated.length());
		appendString(b, d.translated);
	}
	return b;
}

void SeenTextData::loadFromTextFile(const std::string& text) {
	if (text.size()) {
		std::vector<std::string> L = splitLines(text);
		if (L.size() && startsWith(L[0], "type seen")) {
			this->seenNo = static_cast<unsigned>(std::stoul(L[0].substr(9), nullptr, 10));
		}
		size_t i = 2;
		while (i < L.size()) {
			i = nextSep(L, i);
			if (i >= L.size()) break;
			++i;

			SeenTextItem d{};
			while (i < L.size() && !isSep(L[i])) {
				const std::string& s = L[i];
				if (startsWith(s, "index ")) d.index = static_cast<unsigned>(std::stoul(s.substr(6), nullptr, 10));
				else if (startsWith(s, "offset ")) d.offset = static_cast<unsigned>(std::stoul(s.substr(7), nullptr, 10));
				else if (startsWith(s, "length ")) d.length = static_cast<size_t>(std::stoul(s.substr(7), nullptr, 10));
				else if (startsWith(s, REPLACE_FLAG)) d.replaceFlag = true;
				++i;
			}
			if (i < L.size()) ++i;

			std::string origin = readBlock(L, i);
			d.translated = readBlock(L, i);
			if (origin != d.translated) {
				this->items.push_back(std::move(d));
			}
		}
	}
}

void NameData::loadFromBinFile(std::vector<BYTE>::const_iterator begin,
	std::vector<BYTE>::const_iterator end) {
	if (begin != end) {
		DWORD magic1 = 0, magic2 = 0, count = 0;
		if (!readDWORD(begin, end, magic1) || !readDWORD(begin, end, magic2) ||
			!readDWORD(begin, end, count) || magic1 != BIN_MAGIC || magic2 != NAME_BIN_MAGIC) {
			count = 0;
		}
		for (size_t i = 0; i < count; ++i) {
			DWORD os, ts;
			if (!readDWORD(begin, end, os) || !readDWORD(begin, end, ts)) break;
			NameItem d{};
			if (!readString(begin, end, os, d.origin)
				|| !readString(begin, end, ts, d.translated)) break;
			this->data.push_back(std::move(d));
		}
	}
}

std::vector<BYTE> NameData::saveAsBinFile() const {
	std::vector<BYTE> b;
	size_t n = this->data.size();
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, NAME_BIN_MAGIC);
	appendDWORD(b, n);
	for (const NameItem& d : this->data) {
		appendDWORD(b, d.origin.length());
		appendDWORD(b, d.translated.length());
		appendString(b, d.origin);
		appendString(b, d.translated);
	}
	return b;
}

void NameData::loadFromTextFile(const std::string& text) {
	std::vector<NameData> arr;
	if (text.size()) {
		std::vector<std::string> L = splitLines(text);
		size_t i = 2;
		while (i < L.size()) {
			i = nextSep(L, i);
			if (i >= L.size()) break;
			++i;

			while (i < L.size() && !isSep(L[i])) ++i;
			if (i < L.size()) ++i;

			NameItem d{};
			d.origin = readBlock(L, i);
			d.translated = readBlock(L, i);
			if (d.origin != d.translated) {
				this->data.push_back(std::move(d));
			}
		}
	}
}

void PEResourceData::loadFromBinFile(std::vector<BYTE>::const_iterator begin,
	std::vector<BYTE>::const_iterator end) {
	if (begin != end) {
		DWORD magic1 = 0, magic2 = 0, count = 0;
		if (!readDWORD(begin, end, magic1) || !readDWORD(begin, end, magic2) ||
			!readDWORD(begin, end, count) || magic1 != BIN_MAGIC || magic2 != PERES_BIN_MAGIC) {
			count = 0;
		}
		for (size_t i = 0; i < count; ++i) {
			DWORD length, rva, ts, e, s;
			if (!readDWORD(begin, end, length) || !readDWORD(begin, end, rva)
				|| !readDWORD(begin, end, e) || !readDWORD(begin, end, s)
				|| !readDWORD(begin, end, ts)) break;
			PEResourceItem res{};
			if (!readString(begin, end, ts, res.translated)) break;
			res.index = i;
			res.length = length;
			res.rva = rva;
			res.encode = static_cast<PEResourceEncode>(e);
			res.strategy = static_cast<PEResourceStrategy>(s);
			this->data.push_back(std::move(res));
		}
	}
}

std::vector<BYTE> PEResourceData::saveAsBinFile() const {
	std::vector<BYTE> b;
	size_t n = this->data.size();
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, PERES_BIN_MAGIC);
	appendDWORD(b, n);
	for (const PEResourceItem& res : this->data) {
		appendDWORD(b, res.length);
		appendDWORD(b, res.rva);
		appendDWORD(b, static_cast<DWORD>(res.encode));
		appendDWORD(b, static_cast<DWORD>(res.strategy));
		appendDWORD(b, res.translated.length());
		appendString(b, res.translated);
	}
	return b;
}

void PEResourceData::loadFromTextFile(const std::string& text) {
	if (text.length()) {
		std::vector<std::string> L = splitLines(text);
		size_t i = 2;
		while (i < L.size()) {
			i = nextSep(L, i);
			if (i >= L.size()) break;
			++i;

			DWORD rva = 0;
			size_t length = 0;
			PEResourceEncode encode = PEResourceEncode::ENCODE_UNKNOWN;
			PEResourceStrategy strategy = PEResourceStrategy::STRATEGY_UNKNOWN;
			while (i < L.size() && !isSep(L[i])) {
				const std::string& s = L[i];
				if (startsWith(s, "rva ")) rva = std::stoul(s.substr(4), nullptr, 10);
				else if (startsWith(s, "length ")) length = std::stoul(s.substr(7), nullptr, 10);
				else if (startsWith(s, GBK_STR)) encode = PEResourceEncode::GBK;
				else if (startsWith(s, UTF16_STR))encode = PEResourceEncode::UTF16LE;
				else if (startsWith(s, NULL_STRATEGY))strategy = PEResourceStrategy::FILL_NULL;
				else if (startsWith(s, SPACE_STRATEGY)) strategy = PEResourceStrategy::FILL_SPACE;
				i++;
			}
			if (i < L.size()) ++i;

			std::string origin = readBlock(L, i);
			std::string translated = readBlock(L, i);

			if (rva == 0 || length == 0 || origin == translated) {
				continue;
			}

			PEResourceItem res{};
			res.index = this->data.size() + 1;
			res.length = length;
			res.rva = rva;
			res.translated = std::move(translated);
			res.encode = encode;
			res.strategy = strategy;
			this->data.push_back(std::move(res));
		}
	}
}

void FileMappringData::loadFromBinFile(std::vector<BYTE>::const_iterator begin,
	std::vector<BYTE>::const_iterator end) {
	if (begin != end) {
		size_t offset = 0;
		DWORD magic1 = 0, magic2 = 0, count = 0;
		if (!readDWORD(begin, end, magic1) || !readDWORD(begin, end, magic2) ||
			!readDWORD(begin, end, count) || magic1 != BIN_MAGIC || magic2 != FMAP_BIN_MAGIC) {
			count = 0;
		}
		for (size_t i = 0; i < count; ++i) {
			DWORD os, ts;
			if (!readDWORD(begin, end, os) || !readDWORD(begin, end, ts)) break;
			FileMappingItem item{};
			if (!readString(begin, end, os, item.origin) ||
				!readString(begin, end, ts, item.target)) break;
			if (item.origin != item.target) {
				this->data.push_back(std::move(item));
			}
		}
	}
}

std::vector<BYTE> FileMappringData::saveAsBinFile() const {
	std::vector<BYTE> b;
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, FMAP_BIN_MAGIC);
	appendDWORD(b, this->data.size());
	for (const FileMappingItem& res : this->data) {
		appendDWORD(b, res.origin.size());
		appendDWORD(b, res.target.size());
		appendString(b, res.origin);
		appendString(b, res.target);
	}
	return b;
}

void FileMappringData::loadFromTextFile(const std::string& text) {
	if (text.length()) {
		std::vector<std::string> L = splitLines(text);
		size_t i = 2;
		while (i < L.size()) {
			i = nextSep(L, i);
			if (i >= L.size()) break;
			++i;

			FileMappingItem map;
			map.origin = readBlock(L, i);
			map.target = readBlock(L, i);

			if (map.origin != map.target) {
				this->data.push_back(std::move(map));
			}
		}
	}
}

void PatchPack::loadFromBinFile(std::vector<BYTE>::const_iterator begin,
	std::vector<BYTE>::const_iterator end) {
	if (begin == end) {
		return;
	}

	DWORD magic1 = 0, magic2 = 0, pSize = 0;
	if (!readDWORD(begin, end, magic1) || !readDWORD(begin, end, magic2)
		|| magic1 != BIN_MAGIC || magic2 != PACK_BIN_MAGIC) {
		return;
	}

	DWORD size = 0;
	if (readDWORD(begin, end, pSize) && pSize > 0) {
		for (size_t i = 0; i < pSize; ++i) {
			size = 0;
			if (!readDWORD(begin, end, size) || size == 0) return;
			SeenTextData data;
			data.loadFromBinFile(begin, begin + size);
			this->text.push_back(std::move(data));
		}
	}

	size = 0;
	if (readDWORD(begin, end, size) && size > 0) {
		this->name.loadFromBinFile(begin, begin + size);
	}
	size = 0;
	if (readDWORD(begin, end, size) && size > 0) {
		this->res.loadFromBinFile(begin, begin + size);
	}
	size = 0;
	if (readDWORD(begin, end, size) && size > 0) {
		this->fileMap.loadFromBinFile(begin, begin + size);
	}
}

std::vector<BYTE> PatchPack::saveAsBinFile() const {
	std::vector<BYTE> b;
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, PACK_BIN_MAGIC);
	appendDWORD(b, this->text.size());

	for (const SeenTextData& data : this->text) {
		std::vector<BYTE> d = data.saveAsBinFile();
		appendDWORD(b, d.size());
		b.insert(b.end(), std::make_move_iterator(d.begin()), std::make_move_iterator(d.end()));
	}

	std::vector<std::vector<BYTE>> binBytes = {
		this->name.saveAsBinFile(), this->res.saveAsBinFile(),
		this->fileMap.saveAsBinFile()
	};
	for (const auto& v : binBytes) {
		appendDWORD(b, v.size());
		b.insert(b.end(), std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()));
	}
	return b;
}

void PatchPack::update(PatchPack&& other) {
	if (other.name.data.size()) {
		this->name = std::move(other.name);
	}
	if (other.res.data.size()) {
		this->res = std::move(other.res);
	}
	if (other.fileMap.data.size()) {
		this->fileMap = std::move(other.fileMap);
	}
	if (!other.text.size()) {
		return;
	}
	std::set<unsigned> thisSeen;
	std::unordered_map<unsigned, std::reference_wrapper<SeenTextData>> thatSeen;
	thatSeen.reserve(other.text.size());
	for (const auto& v : this->text) {
		thisSeen.insert(v.seenNo);
	}
	for (auto& v : other.text) {
		thatSeen.emplace(v.seenNo, std::ref(v));
	}
	for (auto& v : this->text) {
		auto p = thatSeen.find(v.seenNo);
		if (p != thatSeen.end()) {
			v = std::move(p->second.get());
		}
	}
	for (const auto& [no, ref] : thatSeen) {
		if (thisSeen.find(no) == thisSeen.end()) {
			this->text.push_back(std::move(ref.get()));
		}
	}
}
