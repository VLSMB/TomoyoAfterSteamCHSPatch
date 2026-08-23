#include "data.h"
#include <vector>
#include <set>
#include <utility>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <map>
#include <stack>
#include <new>

template<typename T>
static T* toArrayPointer(const std::vector<T>& vec);

static BYTE* copyBytes(const BYTE* p, size_t n);
static ByteBuffer makeBuffer(const BYTE* p, size_t n);
static std::vector<BYTE> bufferToVec(const ByteBuffer& b);
static void appendDWORD(std::vector<BYTE>& b, DWORD v);
static void appendBytes(std::vector<BYTE>& b, const BYTE* p, size_t n);
static bool readDWORD(const BYTE*& p, const BYTE* const end, DWORD& v);
static std::vector<std::string> splitLines(const BYTE* p, size_t n);
static bool isSep(const std::string& s);
static size_t nextSep(const std::vector<std::string>& L, size_t i);
static std::string readBlock(const std::vector<std::string>& L, size_t& i);
static bool startsWith(const std::string& s, const char* p);
static std::string bufferToStdString(const ByteBuffer& b);
static DWORD seenOffsetToDWORD(unsigned seenNo, unsigned offset);
static void dwordToSeenOffset(DWORD dword, unsigned& seenNo, unsigned& offset);
static bool exactSeenOffset(const char* const bytes, unsigned& seenNo, unsigned& offset);
static std::string vectorToHex(const std::vector<BYTE>& vec);
static std::vector<BYTE> hexToVector(const std::string& hex);
static const char* const pureGetTranslatedText(const char* const text);
static bool byteBufferEquals(const ByteBuffer& b1, const ByteBuffer& b2);

static const char* EMPTY_NAME = "NULL";
static const DWORD BIN_MAGIC = 0x54504C56;
static const DWORD TEXT_BIN_MAGIC = 0x4E454553;
static const DWORD NAME_BIN_MAGIC = 0x454D414E;
static const DWORD PACK_BIN_MAGIC = 0x4B434150;
static const BYTE  TEXT_META_MAGIC = 0xF0;
static const size_t SHOT_TEXT_SIZE = 10;

EXTERN_C void DumpSeenData(RealLiveSeenData* in, SeenDumpData* out) {
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
					data.index = textArray.size() + 1;
					data.offset = i - textBuffer.size();
					data.name.pointer = nameBuffer.empty() ? NULL : toArrayPointer(nameBuffer);
					data.origin.pointer = toArrayPointer(textBuffer);
					data.translated.pointer = toArrayPointer(textBuffer);
					data.name.size = nameBuffer.size();
					data.origin.size = textBuffer.size();
					data.translated.size = textBuffer.size();
					data.length = textBuffer.size();
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

	textArray.erase(std::remove_if(textArray.begin(), textArray.end(), 
		[](SeenPatchData& data) { 
			if (data.length <= 1) {
				FreeSeenPatchData(&data);
				return true;
			}
			return false;
		}),textArray.end());

	out->textData.pointer = toArrayPointer(textArray);
	out->textData.size = textArray.size();
	out->nameData.pointer = toArrayPointer(nameArray);
	out->nameData.size = nameArray.size();
}

EXTERN_C void TextDataToTextFile(SeenPatchDataArray* in, ByteBuffer* out) {
	if (in == NULL) {
		out->pointer = NULL;
		out->size = 0;
		return;
	}
	std::string t;
	size_t n = (in->pointer != NULL) ? in->size : 0;
	t += "type seen" + std::to_string(in->seenNo) + "\n";
	t += "count " + std::to_string(n) + "\n";
	for (size_t i = 0; i < n; ++i) {
		const SeenPatchData& d = in->pointer[i];
		t += "\n====================\n";
		t += "index " + std::to_string(d.index) + "\n";
		t += "offset " + std::to_string(d.offset) + "\n";
		t += "name ";
		if (d.name.pointer != NULL) t.append((const char*)d.name.pointer, d.name.size);
		else t += EMPTY_NAME;
		t += "\n";
		t += "length " + std::to_string(d.length) + "\n";
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
	if (out == NULL) return;
	RtlZeroMemory(out, sizeof(SeenPatchDataArray));
	std::vector<SeenPatchData> arr;
	if (in != NULL && in->pointer != NULL && in->size > 0) {
		std::vector<std::string> L = splitLines(in->pointer, in->size);
		if (L.size() && startsWith(L[0], "type seen")) {
			out->seenNo = (unsigned)strtoul(L[0].c_str() + 9, NULL, 10);
		}
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
				if (startsWith(s, "index ")) d.index = (unsigned)strtoul(s.c_str() + 6, NULL, 10);
				else if (startsWith(s, "offset ")) d.offset = (unsigned)strtoul(s.c_str() + 7, NULL, 10);
				else if (startsWith(s, "name ")) name = s.substr(5);
				else if (startsWith(s, "length ")) d.length = (size_t)strtoul(s.c_str() + 7, NULL, 10);
				++i;
			}
			if (i < L.size()) ++i;

			std::string origin = readBlock(L, i);
			std::string translated = readBlock(L, i);

			if (name == EMPTY_NAME) {
				d.name.pointer = NULL;
				d.name.size = 0;
			} else {
				d.name = makeBuffer((const BYTE*)name.data(), name.size());
			}
			
			d.origin = makeBuffer((const BYTE*)origin.data(), origin.size());
			d.translated = makeBuffer((const BYTE*)translated.data(), translated.size());
			if (byteBufferEquals(d.origin, d.translated)) {
				FreeSeenPatchData(&d);
			} else {
				arr.push_back(d);
			}
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C void TextDataToBinFile(SeenPatchDataArray* in, ByteBuffer* out) {
	if (in == NULL) {
		out->pointer = NULL;
		out->size = 0;
		return;
	}
	std::vector<BYTE> b;
	size_t n = (in->pointer != NULL) ? in->size : 0;
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, TEXT_BIN_MAGIC);
	appendDWORD(b, in->seenNo);
	appendDWORD(b, (DWORD)n);
	for (size_t i = 0; i < n; ++i) {
		const SeenPatchData& d = in->pointer[i];
		appendDWORD(b, (DWORD)d.offset);
		appendDWORD(b, (DWORD)d.length);
		appendDWORD(b, (DWORD)d.translated.size);
		appendBytes(b, d.translated.pointer, d.translated.size);
		if (d.length <= SHOT_TEXT_SIZE) {
			appendDWORD(b, (DWORD)d.origin.size);
			appendBytes(b, d.origin.pointer, d.origin.size);
		}
	}
	out->pointer = toArrayPointer(b);
	out->size = b.size();
}

EXTERN_C void BinFileToTextData(ByteBuffer* in, SeenPatchDataArray* out) {
	std::vector<SeenPatchData> arr;
	RtlZeroMemory(out, sizeof(SeenPatchDataArray));
	DWORD magic1 = 0, magic2 = 0, seenNo = 0, count = 0;
	if (in != NULL && in->pointer != NULL) {
		const BYTE* p = in->pointer;
		const BYTE* const end = p + in->size;
		if (!readDWORD(p, end, magic1) || !readDWORD(p, end, magic2) ||
			!readDWORD(p, end, seenNo) || !readDWORD(p, end, count) ||
			magic1 != BIN_MAGIC || magic2 != TEXT_BIN_MAGIC) {
			count = 0;
		}
		for (size_t i = 0; i < count; ++i) {
			DWORD offset, length, ts;
			if (!readDWORD(p, end, offset) || !readDWORD(p, end, length) 
				|| !readDWORD(p, end, ts)) break;
			if ((size_t)(end - p) < (size_t)ts) break;

			SeenPatchData d;
			RtlZeroMemory(&d, sizeof(d));
			d.index = 0;
			d.offset = offset;
			d.length = length;
			d.name.pointer = NULL;
			d.name.size = 0;
			d.origin.pointer = NULL;
			d.origin.size = 0;
			d.translated = makeBuffer(p, ts); 
			p += ts;

			if (d.length <= SHOT_TEXT_SIZE) {
				DWORD os = 0;
				if (!readDWORD(p, end, os) || os == 0 ||
					(size_t)(end - p) < (size_t)os) break;
				d.origin = makeBuffer(p, os);
				p += os;
			}

			arr.push_back(d);
		}
	}
	if (!arr.empty()) {
		out->pointer = toArrayPointer(arr);
		out->size = arr.size();
		out->seenNo = seenNo;
	} else {
		out->pointer = NULL;
		out->size = 0;
		out->seenNo = 0;
	}
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
			if (byteBufferEquals(d.origin, d.translated)) {
				FreeNameData(&d);
			} else {
				arr.push_back(d);
			}
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C void NameDataToBinFile(NameDataArray* in, ByteBuffer* out) {
	std::vector<BYTE> b;
	size_t n = (in != NULL && in->pointer != NULL) ? in->size : 0;
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, NAME_BIN_MAGIC);
	appendDWORD(b, (DWORD)n);
	for (size_t i = 0; i < n; ++i) {
		const NameData& d = in->pointer[i];
		appendDWORD(b, (DWORD)d.origin.size);
		appendDWORD(b, (DWORD)d.translated.size);
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
		DWORD magic1 = 0, magic2 = 0, count = 0;
		if (!readDWORD(p, end, magic1) || !readDWORD(p, end, magic2) ||
			!readDWORD(p, end, count) || magic1 != BIN_MAGIC || magic2 != NAME_BIN_MAGIC) {
			count = 0;
		}
		for (DWORD i = 0; i < count; ++i) {
			DWORD os, ts;
			if (!readDWORD(p, end, os) || !readDWORD(p, end, ts)) break;
			if ((size_t)(end - p) < (size_t)os + (size_t)ts) break;

			NameData d;
			RtlZeroMemory(&d, sizeof(NameData));
			d.origin = makeBuffer(p, os); p += os;
			d.translated = makeBuffer(p, ts); p += ts;
			arr.push_back(d);
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C void PatchPackToBinFile(PatchPack* in, ByteBuffer* out) {
	RtlZeroMemory(out, sizeof(ByteBuffer));
	if (in == NULL) {
		return;
	}
	std::vector<BYTE> b;
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, PACK_BIN_MAGIC);
	appendDWORD(b, (DWORD)in->pSize);
	ByteBuffer buffer;
	for (size_t i = 0; i < in->pSize; i++) {
		RtlZeroMemory(&buffer, sizeof(ByteBuffer));
		TextDataToBinFile(in->pText[i], &buffer);
		appendDWORD(b, (DWORD)buffer.size);
		appendBytes(b, buffer.pointer, buffer.size);
		FreeByteBuffer(&buffer);
	}
	RtlZeroMemory(&buffer, sizeof(ByteBuffer));
	NameDataToBinFile(in->pName, &buffer);
	appendDWORD(b, (DWORD)buffer.size);
	appendBytes(b, buffer.pointer, buffer.size);
	FreeByteBuffer(&buffer);
	out->pointer = toArrayPointer(b);
	out->size = b.size();
}

EXTERN_C void BinFileToPatchPack(ByteBuffer* in, PatchPack* out) {
	RtlZeroMemory(out, sizeof(PatchPackStruct));
	if (in == NULL || in->pointer == NULL || in->size == 0) {
		return;
	}
	DWORD magic1 = 0, magic2 = 0, pSize = 0;
	const BYTE* p = in->pointer;
	const BYTE* const end = p + in->size;
	if (!readDWORD(p, end, magic1) || !readDWORD(p, end, magic2) || !readDWORD(p, end, pSize)
		|| magic1 != BIN_MAGIC || magic2 != PACK_BIN_MAGIC || pSize <= 0) {
		return;
	}
	out->pSize = pSize;
	out->pText = (SeenPatchDataArray**)malloc(sizeof(SeenPatchDataArray*) * out->pSize);
	if (out->pText == NULL) {
		ExitProcess(1);
	}
	ByteBuffer buffer;
	DWORD size = 0;
	RtlZeroMemory(&buffer, sizeof(ByteBuffer));
	for (size_t i = 0; i < out->pSize; i++) {
		if (!readDWORD(p, end, size) || size == 0) goto fail;
		buffer.pointer = (BYTE*)p;
		buffer.size = size;
		SeenPatchDataArray* text = (SeenPatchDataArray*)malloc(sizeof(SeenPatchDataArray));
		if (text == NULL) goto fail;
		BinFileToTextData(&buffer, text);
		out->pText[i] = text;
		p += size;
	}
	out->pName = (NameDataArray*)malloc(sizeof(NameDataArray));
	if (out->pName == NULL) goto fail;
	if (!readDWORD(p, end, size) || size == 0) goto fail;
	buffer.pointer = (BYTE*)p;
	buffer.size = size;
	BinFileToNameData(&buffer, out->pName);
	return;
fail:
	if (out->pText != NULL) {
		for (size_t i = 0; i < out->pSize; i++) {
			if(out->pText[i] != NULL) FreeSeenPatchDataArray(out->pText[i]);
		}
		if (out->pName != NULL) FreeNameDataArray(out->pName);
		free(out->pText);
	}
	RtlZeroMemory(out, sizeof(PatchPackStruct));
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

EXTERN_C BOOL UpdateSeenPatchDataArray(SeenPatchDataArray** fromDll, size_t szDll, SeenPatchDataArray** fromLocal, size_t szLocal, SeenPatchDataArray*** out, size_t* szOut) {
	std::unordered_map<unsigned, SeenPatchDataArray*> dllMap;
	std::unordered_map<unsigned, SeenPatchDataArray*> localMap;
	std::set<unsigned> seenSet;
	for (size_t i = 0; i < szDll; i++) {
		if (fromDll == NULL || fromDll[i] == NULL) continue;
		seenSet.insert(fromDll[i]->seenNo);
		dllMap.insert(std::make_pair(fromDll[i]->seenNo, fromDll[i]));
	}
	for (size_t i = 0; i < szLocal; i++) {
		if (fromLocal == NULL || fromLocal[i] == NULL) continue;
		seenSet.insert(fromLocal[i]->seenNo);
		localMap.insert(std::make_pair(fromLocal[i]->seenNo, fromLocal[i]));
	}
	*szOut = seenSet.size();
	SeenPatchDataArray** pText = (SeenPatchDataArray**)malloc(sizeof(SeenPatchDataArray*) * seenSet.size());
	*out = pText;
	if (*out == NULL) {
		return FALSE;
	}
	size_t idx = 0;
	for (unsigned seenNo : seenSet) {
		const auto& local = localMap.find(seenNo);
		const auto& dll = dllMap.find(seenNo);
		if (local != localMap.end()) {
			if (dll != dllMap.end()) {
				FreeSeenPatchDataArray(dll->second);
				free(dll->second);
			}
			pText[idx] = local->second;
		} else {
			pText[idx] = dll != dllMap.end() ? dll->second : NULL;
		}
		idx++;
	}
	return TRUE;
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
	data->index = 0;
	data->offset = 0;
	data->length = 0;
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
	array->seenNo = 0;
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

EXTERN_C void FreeSeenDumpData(SeenDumpData* dump) {
	if (dump == NULL) return;
	FreeSeenPatchDataArray(&dump->textData);
	FreeNameDataArray(&dump->nameData);
}

EXTERN_C void FreePatchPack(PatchPack* pack) {
	if (pack->pName != NULL) FreeNameDataArray(pack->pName);
	if (pack->pText != NULL && pack->pSize > 0) {
		for (size_t i = 0; i < pack->pSize; i++) {
			FreeSeenPatchDataArray(pack->pText[i]);
		}
	}
	free(pack->pText);
	pack->pText = NULL;
	pack->pName = NULL;
	pack->pSize = 0;
}

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

template<typename T>
static T* toArrayPointer(const std::vector<T>& vec) {
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

static BYTE* copyBytes(const BYTE* p, size_t n) {
	if (p == NULL || n == 0) return NULL;
	BYTE* r = (BYTE*)malloc(n);
	if (r == NULL) ExitProcess(1);
	memcpy(r, p, n);
	return r;
}

static ByteBuffer makeBuffer(const BYTE* p, size_t n) {
	ByteBuffer b;
	RtlZeroMemory(&b, sizeof(ByteBuffer));
	b.pointer = copyBytes(p, n);
	b.size = n;
	return b;
}

static std::vector<BYTE> bufferToVec(const ByteBuffer& b) {
	if (b.pointer == NULL || b.size == 0) return std::vector<BYTE>();
	return std::vector<BYTE>(b.pointer, b.pointer + b.size);
}

static void appendDWORD(std::vector<BYTE>& b, DWORD v) {
	b.push_back((BYTE)(v & 0xFF));
	b.push_back((BYTE)((v >> 8) & 0xFF));
	b.push_back((BYTE)((v >> 16) & 0xFF));
	b.push_back((BYTE)((v >> 24) & 0xFF));
}

static void appendBytes(std::vector<BYTE>& b, const BYTE* p, size_t n) {
	if (p != NULL && n > 0) b.insert(b.end(), p, p + n);
}

static bool readDWORD(const BYTE*& p, const BYTE* const end, DWORD& v) {
	if (end - p < 4) return false;
	v = (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
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

static std::string bufferToStdString(const ByteBuffer& b) {
	if (b.pointer == NULL || b.size == 0) {
		return "";
	}
	return std::string(reinterpret_cast<const char*>(b.pointer), b.size);
}

static DWORD seenOffsetToDWORD(unsigned seenNo, unsigned offset) {
	return (seenNo << 18) | (offset & 0x3FFFF);
}

static void dwordToSeenOffset(DWORD dword, unsigned& seenNo, unsigned& offset) {
	offset = dword & 0x3FFFF;
	seenNo = dword >> 18;
}

static bool exactSeenOffset(const char* const bytes, unsigned& seenNo, unsigned& offset) {
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

static std::string vectorToHex(const std::vector<BYTE>& vec) {
	std::string hex;
	hex.reserve(vec.size() * 2);
	char buf[3];
	for (BYTE b : vec) {
		snprintf(buf, sizeof(buf), "%02X", b);
		hex += buf;
	}
	return hex;
}

static std::vector<BYTE> hexToVector(const std::string& hex) {
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

static bool byteBufferEquals(const ByteBuffer& b1, const ByteBuffer& b2) {
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
