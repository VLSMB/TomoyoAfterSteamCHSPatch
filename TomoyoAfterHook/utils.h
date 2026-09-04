#ifndef __VLSMB_UTILS_H
#define __VLSMB_UTILS_H

#ifndef __cplusplus
#error "This header is for CPP only."
#endif

#include "struct.h"
#include <vector>
#include <string>

BYTE* copyBytes(const BYTE* p, size_t n);
ByteBuffer makeBuffer(const BYTE* p, size_t n);
std::vector<BYTE> bufferToVec(const ByteBuffer& b);
void appendDWORD(std::vector<BYTE>& b, DWORD v);
void appendBytes(std::vector<BYTE>& b, const BYTE* p, size_t n);
bool readDWORD(const BYTE*& p, const BYTE* const end, DWORD& v);
std::vector<std::string> splitLines(const BYTE* p, size_t n);
bool isSep(const std::string& s);
size_t nextSep(const std::vector<std::string>& L, size_t i);
std::string readBlock(const std::vector<std::string>& L, size_t& i);
bool startsWith(const std::string& s, const char* p);
std::string bufferToStdString(const ByteBuffer& b);
DWORD seenOffsetToDWORD(unsigned seenNo, unsigned offset);
void dwordToSeenOffset(DWORD dword, unsigned& seenNo, unsigned& offset);
bool exactSeenOffset(const char* const bytes, unsigned& seenNo, unsigned& offset);
std::string vectorToHex(const std::vector<BYTE>& vec);
std::vector<BYTE> hexToVector(const std::string& hex);
bool byteBufferEquals(const ByteBuffer& b1, const ByteBuffer& b2);
std::string transferToGbk(const std::string& str);
bool startsWith(const std::string& str, const std::string& prefix);
std::string removePrefix(const std::string& str, const std::string& prefix);
bool extractRlDebugSuffix(const std::string& str, std::string& prefix, std::string& suffix);

template<typename T>
T* toArrayPointer(const std::vector<T>& vec) {
	static_assert(std::is_trivially_copyable_v<T>, "type must be trivially copyable");
	if (vec.empty()) {
		return NULL;
	}
	T* arr = (T*)malloc(vec.size() * sizeof(T));
	if (arr == NULL) {
		ExitProcess(1);
	}
	if (!vec.empty()) {
		memcpy(arr, vec.data(), vec.size() * sizeof(T));
	}
	return arr;
}

#endif
