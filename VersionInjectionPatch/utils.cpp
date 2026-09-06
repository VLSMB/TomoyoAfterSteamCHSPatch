#include "utils.h"
#include <Windows.h>
#include <regex>

BYTE* copyBytes(const BYTE* p, size_t n) {
	if (p == NULL || n == 0) return NULL;
	BYTE* r = (BYTE*)malloc(n);
	memcpy(r, p, n);
	return r;
}

ByteBuffer makeBuffer(const BYTE* p, size_t n) {
	ByteBuffer b;
	RtlZeroMemory(&b, sizeof(ByteBuffer));
	b.pointer = copyBytes(p, n);
	b.size = n;
	return b;
}

std::vector<BYTE> bufferToVec(const ByteBuffer& b) {
	if (b.pointer == NULL || b.size == 0) return std::vector<BYTE>();
	return std::vector<BYTE>(b.pointer, b.pointer + b.size);
}

void appendDWORD(std::vector<BYTE>& b, DWORD v) {
	b.push_back((BYTE)(v & 0xFF));
	b.push_back((BYTE)((v >> 8) & 0xFF));
	b.push_back((BYTE)((v >> 16) & 0xFF));
	b.push_back((BYTE)((v >> 24) & 0xFF));
}

void appendBytes(std::vector<BYTE>& b, const BYTE* p, size_t n) {
	if (p != NULL && n > 0) b.insert(b.end(), p, p + n);
}

bool readDWORD(const BYTE*& p, const BYTE* const end, DWORD& v) {
	if (end - p < 4) return false;
	v = (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
	p += 4;
	return true;
}

std::vector<std::string> splitLines(const BYTE* p, size_t n) {
	std::vector<std::string> L;
	std::string cur;
	for (size_t i = 0; i < n; ++i) {
		char c = (char)p[i];
		if (c == '\n') {
			if (!cur.empty() && cur.back() == '\r') cur.pop_back();
			L.push_back(std::move(cur));
			cur.clear();
		} else {
			cur.push_back(c);
		}
	}
	if (!cur.empty()) {
		if (cur.back() == '\r') cur.pop_back();
		L.push_back(std::move(cur));
	}
	return L;
}

bool isSep(const std::string& s) {
	return !s.empty() && s[0] == '=';
}

size_t nextSep(const std::vector<std::string>& L, size_t i) {
	while (i < L.size() && !isSep(L[i])) ++i;
	return i;
}

std::string readBlock(const std::vector<std::string>& L, size_t& i) {
	std::string s;
	while (i < L.size() && !isSep(L[i])) {
		if (!s.empty()) s += '\n';
		s += L[i];
		++i;
	}
	if (i < L.size()) ++i;
	return s;
}

bool startsWith(const std::string& s, const char* p) {
	size_t n = strlen(p);
	return s.size() >= n && memcmp(s.data(), p, n) == 0;
}

std::string bufferToStdString(const ByteBuffer& b) {
	if (b.pointer == NULL || b.size == 0) {
		return "";
	}
	return std::string(reinterpret_cast<const char*>(b.pointer), b.size);
}

DWORD seenOffsetToDWORD(unsigned seenNo, unsigned offset) {
	return (seenNo << 18) | (offset & 0x3FFFF);
}

void dwordToSeenOffset(DWORD dword, unsigned& seenNo, unsigned& offset) {
	offset = dword & 0x3FFFF;
	seenNo = dword >> 18;
}

bool exactSeenOffset(const char* const bytes, unsigned& seenNo, unsigned& offset) {
	size_t count = 0;
	const char* p = bytes;
	while (count < SHOT_TEXT_SIZE && *p) {
		p++;
		count++;
	}
	if (count < SHOT_TEXT_SIZE) {
		return false;
	}
	std::vector<BYTE> vec = hexToVector(std::string(bytes, SHOT_TEXT_SIZE));
	DWORD dword = vec[1] | ((DWORD)vec[2] << 8) | ((DWORD)vec[3] << 16) | ((DWORD)vec[4] << 24);
	dwordToSeenOffset(dword, seenNo, offset);
	return true;
}

std::string vectorToHex(const std::vector<BYTE>& vec) {
	std::string hex;
	hex.reserve(vec.size() * 2);
	char buf[3];
	for (BYTE b : vec) {
		snprintf(buf, sizeof(buf), "%02X", b);
		hex += buf;
	}
	return hex;
}

std::vector<BYTE> hexToVector(const std::string& hex) {
	std::vector<BYTE> vec;
	vec.reserve(hex.size() / 2);
	for (size_t i = 0; i < hex.size(); i += 2) {
		std::string byteStr = hex.substr(i, 2);
		unsigned value;
		sscanf_s(byteStr.c_str(), "%02X", &value);
		vec.push_back(static_cast<BYTE>(value));
	}
	return vec;
}

bool byteBufferEquals(const ByteBuffer& b1, const ByteBuffer& b2) {
	if (b1.size != b2.size) return false;
	if (b1.size == 0 || b1.pointer == b2.pointer) return true;
	if (b1.pointer == NULL || b2.pointer == NULL) return false;
	for (size_t i = 0; i < b1.size; i++) {
		if (b1.pointer[i] != b2.pointer[i]) {
			return false;
		}
	}
	return true;
}

std::string transferToGbk(const std::string& str) {
	if (str.empty()) return str;
	int wLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), NULL, 0);
	if (wLen <= 0) {
		return str;
	}
	std::vector<WCHAR> wBuf(wLen);
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), wBuf.data(), wLen);
	int len = WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wBuf.data(), wLen, NULL, NULL, NULL, NULL);
	if (len <= 0) {
		return str;
	}
	std::vector<char> buf(len);
	WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wBuf.data(), wLen, buf.data(), len, NULL, NULL);
	return std::string(buf.data(), buf.size());
}

bool startsWith(const std::string& str, const std::string& prefix) {
	if (prefix.length() > str.length()) return false;
	return str.compare(0, prefix.length(), prefix) == 0;
}

std::string removePrefix(const std::string& str, const std::string& prefix) {
	if (startsWith(str, prefix)) {
		return str.substr(prefix.length());
	}
	return str;
}

bool extractRlDebugSuffix(const std::string& str, std::string& prefix, std::string& suffix) {
	std::regex pattern(
		R"(Seen\d+\(\d+\)\s*Call\s*\(\d+\)\s*GS\s*\(\d+\)$)",
		std::regex::icase
	);

	std::smatch matches;
	if (std::regex_search(str, matches, pattern)) {
		suffix = matches.str(0);
		prefix = str.substr(0, matches.position(0));
		return true;
	}
	prefix = str;
	suffix.clear();
	return false;
}
