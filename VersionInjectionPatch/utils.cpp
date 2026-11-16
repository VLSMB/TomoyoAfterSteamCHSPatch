#include "utils.h"
#include <Windows.h>

void appendDWORD(std::vector<BYTE>& b, DWORD v) {
	b.push_back((BYTE)(v & 0xFF));
	b.push_back((BYTE)((v >> 8) & 0xFF));
	b.push_back((BYTE)((v >> 16) & 0xFF));
	b.push_back((BYTE)((v >> 24) & 0xFF));
}

void appendString(std::vector<BYTE>& b, const std::string& text) {
	b.insert(b.end(), text.begin(), text.end());
}

bool readDWORD(std::vector<BYTE>::const_iterator& begin,
	const std::vector<BYTE>::const_iterator& end,
	DWORD& v) {
	if (std::distance(begin, end) < 4) return false;

	v = static_cast<DWORD>(begin[0]) |
		(static_cast<DWORD>(begin[1]) << 8) |
		(static_cast<DWORD>(begin[2]) << 16) |
		(static_cast<DWORD>(begin[3]) << 24);

	begin += 4;
	return true;
}

bool readString(std::vector<BYTE>::const_iterator& begin,
	const std::vector<BYTE>::const_iterator& end,
	size_t len, std::string& out) {
	if (std::distance(begin, end) < static_cast<std::ptrdiff_t>(len)) return false;

	const BYTE* p = &*begin;
	out.assign(reinterpret_cast<const char*>(p), len);

	begin += len;
	return true;
}

std::vector<std::string> splitLines(const std::string& p) {
	std::vector<std::string> L;
	std::string cur;
	for (size_t i = 0; i < p.length(); ++i) {
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

bool GbkToUtf16Le(const std::string& gbk, std::string& out) {
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