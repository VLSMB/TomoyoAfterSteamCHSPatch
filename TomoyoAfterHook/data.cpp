#include "data.h"
#include "utils.h"
#include <set>
#include <unordered_map>

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
			std::string translated = transferToGbk(readBlock(L, i));

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
			std::string translated = transferToGbk(readBlock(L, i));

			NameData d;
			RtlZeroMemory(&d, sizeof(NameData));
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
	result->size = unique.size();
	if (unique.empty()) {
		result->pointer = NULL;
	} else {
		result->pointer = (NameData*)malloc(unique.size() * sizeof(NameData));
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
