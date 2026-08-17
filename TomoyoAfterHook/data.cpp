#include "data.h"
#include <vector>
#include <set>
#include <utility>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <type_traits>

template<typename T>
T* toArrayPointer(const std::vector<T>& vec);

static BYTE* copyBytes(const BYTE* p, size_t n);
static ByteBuffer makeBuffer(const BYTE* p, size_t n);
static std::vector<BYTE> bufferToVec(const ByteBuffer& b);
static void appendU32(std::vector<BYTE>& b, uint32_t v);
static void appendBytes(std::vector<BYTE>& b, const BYTE* p, size_t n);
static bool readU32(const BYTE*& p, const BYTE* end, uint32_t& v);
static std::vector<std::string> splitLines(const BYTE* p, size_t n);
static bool isSep(const std::string& s);
static size_t nextSep(const std::vector<std::string>& L, size_t i);
static std::string readBlock(const std::vector<std::string>& L, size_t& i);
static bool startsWith(const std::string& s, const char* p);

EXTERN_C void DumpSeenData(unsigned seenNo, RealLiveSeenData* in, SeenDumpData* out) {
	BYTE* const data = in->decompressed_data;
	const size_t size = in->decompressed_size;
	RtlZeroMemory(out, sizeof(SeenDumpData));
	std::vector<SeenPatchData> textArray;
	std::vector<BYTE> textBuffer;
	std::vector<BYTE> nameBuffer;
	std::set<std::vector<BYTE>> nameSet;

	bool inQuote = false;
	bool inName = false;
	size_t i = 0;
	while (i < size) {
		const BYTE b = data[i];
		if (inQuote) {
			if (b == '\\' && i + 1 < size && data[i + 1] == '"') {
				textBuffer.push_back('\\');
				textBuffer.push_back('"');
				i += 2;
				continue;
			}
			if (b == '"') {
				bool hasInvalidChar = false;
				for (size_t j = 0; j < textBuffer.size(); ++j) {
					BYTE c = textBuffer[j];
					if (!(c >= 0x20 && c <= 0x7E)) {
						hasInvalidChar = true;
						break;
					}
				}
				if (hasInvalidChar) {
					textBuffer.clear();
					if (i >= 2 && data[i - 2] == 0x81 && data[i - 1] == 0x79) {
						inQuote = false;
						i -= 2;
					} else {
						inQuote = true;
						++i;
					}
					continue;
				}
				inQuote = false;
				if (inName) {
					nameBuffer.insert(nameBuffer.end(), textBuffer.begin(), textBuffer.end());
					nameSet.insert(std::move(textBuffer));
				} else {
					SeenPatchData data;
					RtlZeroMemory(&data, sizeof(SeenPatchData));
					data.number = seenNo;
					data.offset = i - textBuffer.size();
					data.name.pointer = nameBuffer.empty() ? NULL : toArrayPointer(nameBuffer);
					data.origin.pointer = toArrayPointer(textBuffer);
					data.translated.pointer = toArrayPointer(textBuffer);
					data.name.size = nameBuffer.size();
					data.origin.size = textBuffer.size();
					data.translated.size = textBuffer.size();
					textArray.push_back(data);
					nameBuffer.clear();
				}
				textBuffer.clear();
			} else {
				textBuffer.push_back(b);
			}
			++i;
			continue;
		}

		if (b == 0x81 && i + 1 < size) {
			if (data[i + 1] == 0x79) {
				inName = true;
				i += 2;
				continue;
			}
			if (data[i + 1] == 0x7A) {
				inName = false;
				i += 2;
				continue;
			}
		}

		if (b == '@' || b == '!' || b == '$' || b == '#') {
			i += 3;
			continue;
		}

		if (b == '"') {
			textBuffer.clear();
			inQuote = true;
		}
		++i;
	}

	std::vector<NameData> nameArray;
	for (const std::vector<BYTE>& vec : nameSet) {
		if (vec.empty()) continue;
		NameData data;
		RtlZeroMemory(&data, sizeof(NameData));
		data.origin.pointer = toArrayPointer(vec);
		data.translated.pointer = toArrayPointer(vec);
		data.origin.size = vec.size();
		data.translated.size = vec.size();
		nameArray.push_back(data);
	}

	out->number = seenNo;
	out->textData.pointer = toArrayPointer(textArray);
	out->textData.size = textArray.size();
	out->nameData.pointer = toArrayPointer(nameArray);
	out->nameData.size = nameArray.size();
}

EXTERN_C void TextDataToTextFile(SeenPatchDataArray* in, ByteBuffer* out) {
	std::string t;
	size_t n = (in != NULL && in->pointer != NULL) ? in->size : 0;
	t += "type seen\n";
	t += "count " + std::to_string(n) + "\n";
	for (size_t i = 0; i < n; ++i) {
		const SeenPatchData& d = in->pointer[i];
		t += "\n====================\n";
		t += "number " + std::to_string(d.number) + "\n";
		t += "offset " + std::to_string(d.offset) + "\n";
		t += "name ";
		if (d.name.pointer != NULL) t.append((const char*)d.name.pointer, d.name.size);
		else t.append("NULL", 4);
		t += "\n";
		t += "length " + std::to_string(d.origin.size) + "\n";
		t += "====================\n";
		if (d.origin.pointer != NULL) t.append((const char*)d.origin.pointer, d.origin.size);
		t += "\n====================\n";
		if (d.translated.pointer != NULL) t.append((const char*)d.translated.pointer, d.translated.size);
		t += "\n====================\n";
	}
	std::vector<BYTE> buf(t.begin(), t.end());
	out->pointer = toArrayPointer(buf);
	out->size = buf.size();
}

EXTERN_C void TextFileToTextData(ByteBuffer* in, SeenPatchDataArray* out) {
	std::vector<SeenPatchData> arr;
	if (in != NULL && in->pointer != NULL && in->size > 0) {
		std::vector<std::string> L = splitLines(in->pointer, in->size);
		size_t i = 2;
		while (i < L.size()) {
			i = nextSep(L, i);
			if (i >= L.size()) break;
			++i;

			SeenPatchData d;
			RtlZeroMemory(&d, sizeof(d));
			std::string name;
			while (i < L.size() && !isSep(L[i])) {
				const std::string& s = L[i];
				if (startsWith(s, "number ")) d.number = (unsigned)strtoul(s.c_str() + 7, NULL, 10);
				else if (startsWith(s, "offset ")) d.offset = (unsigned)strtoul(s.c_str() + 7, NULL, 10);
				else if (startsWith(s, "name ")) name = s.substr(5);
				++i;
			}
			if (i < L.size()) ++i;

			std::string origin = readBlock(L, i);
			std::string translated = readBlock(L, i);

			d.name = makeBuffer((const BYTE*)name.data(), name.size());
			if (d.name.size == 4 && d.name.pointer[0] == 'N' && d.name.pointer[1] == 'U'
				&& d.name.pointer[2] == 'L' && d.name.pointer[3] == 'L') {
				d.name.size = 0;
				d.name.pointer = NULL;
			}
			d.origin = makeBuffer((const BYTE*)origin.data(), origin.size());
			d.translated = makeBuffer((const BYTE*)translated.data(), translated.size());
			arr.push_back(d);
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C void TextDataToBinFile(SeenPatchDataArray* in, ByteBuffer* out) {
	std::vector<BYTE> b;
	size_t n = (in != NULL && in->pointer != NULL) ? in->size : 0;
	appendU32(b, (uint32_t)n);
	for (size_t i = 0; i < n; ++i) {
		const SeenPatchData& d = in->pointer[i];
		appendU32(b, (uint32_t)d.number);
		appendU32(b, (uint32_t)d.offset);
		appendU32(b, (uint32_t)d.name.size);
		appendU32(b, (uint32_t)d.origin.size);
		appendU32(b, (uint32_t)d.translated.size);
		appendBytes(b, d.name.pointer, d.name.size);
		appendBytes(b, d.origin.pointer, d.origin.size);
		appendBytes(b, d.translated.pointer, d.translated.size);
	}
	out->pointer = toArrayPointer(b);
	out->size = b.size();
}

EXTERN_C void BinFileToTextData(ByteBuffer* in, SeenPatchDataArray* out) {
	std::vector<SeenPatchData> arr;
	if (in != NULL && in->pointer != NULL) {
		const BYTE* p = in->pointer;
		const BYTE* end = p + in->size;
		uint32_t count = 0;
		if (!readU32(p, end, count)) count = 0;
		for (uint32_t i = 0; i < count; ++i) {
			uint32_t number, offset, ns, os, ts;
			if (!readU32(p, end, number) || !readU32(p, end, offset) ||
				!readU32(p, end, ns) || !readU32(p, end, os) || !readU32(p, end, ts)) break;
			if ((size_t)(end - p) < (size_t)ns + (size_t)os + (size_t)ts) break;

			SeenPatchData d;
			RtlZeroMemory(&d, sizeof(d));
			d.number = number;
			d.offset = offset;
			d.name = makeBuffer(p, ns); p += ns;
			d.origin = makeBuffer(p, os); p += os;
			d.translated = makeBuffer(p, ts); p += ts;
			arr.push_back(d);
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C void NameDataToTextFile(NameDataArray* in, ByteBuffer* out) {
	std::string t;
	size_t n = (in != NULL && in->pointer != NULL) ? in->size : 0;
	t += "type name\n";
	t += "count " + std::to_string(n) + "\n";
	for (size_t i = 0; i < n; ++i) {
		const NameData& d = in->pointer[i];
		t += "\n====================\n";
		t += "length " + std::to_string(d.origin.size) + "\n";
		t += "====================\n";
		if (d.origin.pointer != NULL) t.append((const char*)d.origin.pointer, d.origin.size);
		t += "\n====================\n";
		if (d.translated.pointer != NULL) t.append((const char*)d.translated.pointer, d.translated.size);
		t += "\n====================\n";
	}
	std::vector<BYTE> buf(t.begin(), t.end());
	out->pointer = toArrayPointer(buf);
	out->size = buf.size();
}

EXTERN_C void TextFileToNameData(ByteBuffer* in, NameDataArray* out) {
	std::vector<NameData> arr;
	if (in != NULL && in->pointer != NULL && in->size > 0) {
		std::vector<std::string> L = splitLines(in->pointer, in->size);
		size_t i = 2;
		while (i < L.size()) {
			i = nextSep(L, i);
			if (i >= L.size()) break;
			++i;

			while (i < L.size() && !isSep(L[i])) ++i;
			if (i < L.size()) ++i;

			std::string origin = readBlock(L, i);
			std::string translated = readBlock(L, i);

			NameData d;
			d.origin = makeBuffer((const BYTE*)origin.data(), origin.size());
			d.translated = makeBuffer((const BYTE*)translated.data(), translated.size());
			arr.push_back(d);
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C void NameDataToBinFile(NameDataArray* in, ByteBuffer* out) {
	std::vector<BYTE> b;
	size_t n = (in != NULL && in->pointer != NULL) ? in->size : 0;
	appendU32(b, (uint32_t)n);
	for (size_t i = 0; i < n; ++i) {
		const NameData& d = in->pointer[i];
		appendU32(b, (uint32_t)d.origin.size);
		appendU32(b, (uint32_t)d.translated.size);
		appendBytes(b, d.origin.pointer, d.origin.size);
		appendBytes(b, d.translated.pointer, d.translated.size);
	}
	out->pointer = toArrayPointer(b);
	out->size = b.size();
}

EXTERN_C void BinFileToNameData(ByteBuffer* in, NameDataArray* out) {
	std::vector<NameData> arr;
	if (in != NULL && in->pointer != NULL) {
		const BYTE* p = in->pointer;
		const BYTE* end = p + in->size;
		uint32_t count = 0;
		if (!readU32(p, end, count)) count = 0;
		for (uint32_t i = 0; i < count; ++i) {
			uint32_t os, ts;
			if (!readU32(p, end, os) || !readU32(p, end, ts)) break;
			if ((size_t)(end - p) < (size_t)os + (size_t)ts) break;

			NameData d;
			d.origin = makeBuffer(p, os); p += os;
			d.translated = makeBuffer(p, ts); p += ts;
			arr.push_back(d);
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C NameDataArray* MergeNameDataArray(NameDataArray** arrayList, size_t arraySize) {
	std::set<std::pair<std::vector<BYTE>, std::vector<BYTE>>> seen;
	std::vector<std::pair<std::vector<BYTE>, std::vector<BYTE>>> unique;

	if (arrayList != NULL) {
		for (size_t i = 0; i < arraySize; ++i) {
			NameDataArray* a = arrayList[i];
			if (a == NULL || a->pointer == NULL) continue;
			for (size_t j = 0; j < a->size; ++j) {
				std::pair<std::vector<BYTE>, std::vector<BYTE>> key(
					bufferToVec(a->pointer[j].origin),
					bufferToVec(a->pointer[j].translated));
				if (seen.insert(key).second) unique.push_back(std::move(key));
			}
		}
	}

	NameDataArray* result = (NameDataArray*)malloc(sizeof(NameDataArray));
	if (result == NULL) ExitProcess(1);
	result->size = unique.size();
	if (unique.empty()) {
		result->pointer = NULL;
	} else {
		result->pointer = (NameData*)malloc(unique.size() * sizeof(NameData));
		if (result->pointer == NULL) ExitProcess(1);
		for (size_t i = 0; i < unique.size(); ++i) {
			result->pointer[i].origin = makeBuffer(unique[i].first.data(), unique[i].first.size());
			result->pointer[i].translated = makeBuffer(unique[i].second.data(), unique[i].second.size());
		}
	}
	return result;
}

EXTERN_C void FreeByteBuffer(ByteBuffer* buf) {
	if (buf == NULL) return;
	free(buf->pointer);
	buf->pointer = NULL;
	buf->size = 0;
}

EXTERN_C void FreeSeenPatchData(SeenPatchData* data) {
	if (data == NULL) return;
	FreeByteBuffer(&data->name);
	FreeByteBuffer(&data->origin);
	FreeByteBuffer(&data->translated);
	data->number = 0;
	data->offset = 0;
}

EXTERN_C void FreeSeenPatchDataArray(SeenPatchDataArray* array) {
	if (array == NULL) return;
	if (array->pointer != NULL) {
		for (size_t i = 0; i < array->size; ++i) {
			FreeSeenPatchData(&array->pointer[i]);
		}
		free(array->pointer);
		array->pointer = NULL;
	}
	array->size = 0;
}

EXTERN_C void FreeNameData(NameData* data) {
	if (data == NULL) return;
	FreeByteBuffer(&data->origin);
	FreeByteBuffer(&data->translated);
}

EXTERN_C void FreeNameDataArray(NameDataArray* array) {
	if (array == NULL) return;
	if (array->pointer != NULL) {
		for (size_t i = 0; i < array->size; ++i) {
			FreeNameData(&array->pointer[i]);
		}
		free(array->pointer);
		array->pointer = NULL;
	}
	array->size = 0;
}

EXTERN_C void FreeSingleWordExtendMap(SingleWordExtendMap* map) {
	if (map == NULL) return;
	FreeByteBuffer(&map->sentence);
	map->word = 0;
}

EXTERN_C void FreeSeenDumpData(SeenDumpData* dump) {
	if (dump == NULL) return;
	FreeSeenPatchDataArray(&dump->textData);
	FreeNameDataArray(&dump->nameData);
	dump->number = 0;
}

template<typename T>
T* toArrayPointer(const std::vector<T>& vec) {
	static_assert(std::is_trivially_copyable_v<T>, "type must be trivially copyable");
	T* arr = (T*)malloc(vec.size() * sizeof(T));
	if (arr == NULL) {
		ExitProcess(1);
	}
	if (!vec.empty()) {
		memcpy(arr, vec.data(), vec.size() * sizeof(T));
	}
	return arr;
}

static BYTE* copyBytes(const BYTE* p, size_t n) {
	if (p == NULL || n == 0) return NULL;
	BYTE* r = (BYTE*)malloc(n);
	if (r == NULL) ExitProcess(1);
	memcpy(r, p, n);
	return r;
}

static ByteBuffer makeBuffer(const BYTE* p, size_t n) {
	ByteBuffer b;
	b.pointer = copyBytes(p, n);
	b.size = n;
	return b;
}

static std::vector<BYTE> bufferToVec(const ByteBuffer& b) {
	if (b.pointer == NULL || b.size == 0) return std::vector<BYTE>();
	return std::vector<BYTE>(b.pointer, b.pointer + b.size);
}

static void appendU32(std::vector<BYTE>& b, uint32_t v) {
	b.push_back((BYTE)(v & 0xFF));
	b.push_back((BYTE)((v >> 8) & 0xFF));
	b.push_back((BYTE)((v >> 16) & 0xFF));
	b.push_back((BYTE)((v >> 24) & 0xFF));
}

static void appendBytes(std::vector<BYTE>& b, const BYTE* p, size_t n) {
	if (p != NULL && n > 0) b.insert(b.end(), p, p + n);
}

static bool readU32(const BYTE*& p, const BYTE* end, uint32_t& v) {
	if (end - p < 4) return false;
	v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	p += 4;
	return true;
}

static std::vector<std::string> splitLines(const BYTE* p, size_t n) {
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

static bool isSep(const std::string& s) {
	return !s.empty() && s[0] == '=';
}

static size_t nextSep(const std::vector<std::string>& L, size_t i) {
	while (i < L.size() && !isSep(L[i])) ++i;
	return i;
}

static std::string readBlock(const std::vector<std::string>& L, size_t& i) {
	std::string s;
	while (i < L.size() && !isSep(L[i])) {
		if (!s.empty()) s += '\n';
		s += L[i];
		++i;
	}
	if (i < L.size()) ++i;
	return s;
}

static bool startsWith(const std::string& s, const char* p) {
	size_t n = strlen(p);
	return s.size() >= n && memcmp(s.data(), p, n) == 0;
}
