#include "data.h"
#include "utils.h"
#include <set>
#include <unordered_map>

EXTERN_C void transferBufferToGbk(ByteBuffer* in, ByteBuffer* out) {
	RtlZeroMemory(out, sizeof(ByteBuffer));
	std::string str = transferToGbk(bufferToStdString(*in));
	out->size = str.size();
	out->pointer = (BYTE*)malloc(out->size * sizeof(BYTE));
	memcpy(out->pointer, str.c_str(), out->size);
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
				else if (startsWith(s, REPLACE_FLAG)) d.replaceFlag = 1;
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
		appendDWORD(b, (DWORD)d.replaceFlag);
		appendDWORD(b, (DWORD)d.offset);
		appendDWORD(b, (DWORD)d.length);
		appendDWORD(b, (DWORD)d.translated.size);
		appendBytes(b, d.translated.pointer, d.translated.size);
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
			DWORD replaceFlag, offset, length, ts;
			if (!readDWORD(p, end, replaceFlag) || !readDWORD(p, end, offset) 
				|| !readDWORD(p, end, length) || !readDWORD(p, end, ts)) break;
			if ((size_t)(end - p) < (size_t)ts) break;

			SeenPatchData d;
			RtlZeroMemory(&d, sizeof(d));
			d.replaceFlag = replaceFlag ? 1 : 0;
			d.index = 0;
			d.offset = offset;
			d.length = length;
			d.name.pointer = NULL;
			d.name.size = 0;
			d.origin.pointer = NULL;
			d.origin.size = 0;
			d.translated = makeBuffer(p, ts); 
			p += ts;
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

EXTERN_C void TextFileToPEResource(ByteBuffer* in, PEResourceArray* out) {
	std::vector<PEResource> arr;
	if (in != NULL && in->pointer != NULL && in->size > 0) {
		std::vector<std::string> L = splitLines(in->pointer, in->size);
		size_t i = 2;
		while (i < L.size()) {
			i = nextSep(L, i);
			if (i >= L.size()) break;
			++i;

			DWORD rva = 0;
			size_t length = 0;
			PEResourceEncode encode = ENCODE_UNKNOWN;
			PEResourceStrategy strategy = STRATEGY_UNKNOWN;
			while (i < L.size() && !isSep(L[i])) {
				const std::string& s = L[i];
				if (startsWith(s, "rva ")) rva = (DWORD)strtoul(s.c_str() + 4, NULL, 10);
				else if (startsWith(s, "length ")) length = (size_t)strtoul(s.c_str() + 7, NULL, 10);
				else if (startsWith(s, GBK_STR)) encode = GBK;
				else if (startsWith(s, UTF16_STR))encode = UTF16LE;
				else if (startsWith(s, NULL_STRATEGY))strategy = FILL_NULL;
				else if (startsWith(s, SPACE_STRATEGY)) strategy = FILL_SPACE;
				i++;
			}
			if (i < L.size()) ++i;

			std::string origin = readBlock(L, i);
			std::string translated = readBlock(L, i);

			if (rva == 0 || length == 0 || origin == translated) {
				continue;
			}

			PEResource res;
			RtlZeroMemory(&res, sizeof(PEResource));
			res.index = arr.size() + 1;
			res.length = length;
			res.rva = rva;
			res.origin = makeBuffer((const BYTE*)origin.data(), origin.size());
			res.translated = makeBuffer((const BYTE*)translated.data(), translated.size());
			res.encode = encode;
			res.strategy = strategy;
			arr.push_back(res);
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C void PEResourceToBinFile(PEResourceArray* in, ByteBuffer* out) {
	std::vector<BYTE> b;
	size_t n = (in != NULL && in->pointer != NULL) ? in->size : 0;
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, PERES_BIN_MAGIC);
	appendDWORD(b, (DWORD)n);
	for (size_t i = 0; i < n; ++i) {
		const PEResource& res = in->pointer[i];
		appendDWORD(b, (DWORD)res.length);
		appendDWORD(b, (DWORD)res.rva);
		appendDWORD(b, (DWORD)res.encode);
		appendDWORD(b, (DWORD)res.strategy);
		appendDWORD(b, (DWORD)res.translated.size);
		appendBytes(b, res.translated.pointer, res.translated.size);
	}
	out->pointer = toArrayPointer(b);
	out->size = b.size();
}

EXTERN_C void BinFileToPEResource(ByteBuffer* in, PEResourceArray* out) {
	std::vector<PEResource> arr;
	if (in != NULL && in->pointer != NULL) {
		const BYTE* p = in->pointer;
		const BYTE* end = p + in->size;
		DWORD magic1 = 0, magic2 = 0, count = 0;
		if (!readDWORD(p, end, magic1) || !readDWORD(p, end, magic2) ||
			!readDWORD(p, end, count) || magic1 != BIN_MAGIC || magic2 != PERES_BIN_MAGIC) {
			count = 0;
		}
		for (DWORD i = 0; i < count; ++i) {
			DWORD length, rva, ts, e, s;
			if (!readDWORD(p, end, length) || !readDWORD(p, end, rva)
				|| !readDWORD(p, end, e) || !readDWORD(p, end, s)
				|| !readDWORD(p, end, ts)) break;
			if ((size_t)(end - p) < (size_t)ts) break;
			PEResource res;
			RtlZeroMemory(&res, sizeof(PEResource));
			res.index = i;
			res.length = length;
			res.rva = rva;
			res.encode = (PEResourceEncode)e;
			res.strategy = (PEResourceStrategy)s;
			res.translated = makeBuffer(p, ts); p += ts;
			arr.push_back(res);
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C void TextFileToFileMapping(ByteBuffer* in, FileMappingArray* out) {
	std::vector<FileMapping> arr;
	if (in != NULL && in->pointer != NULL && in->size > 0) {
		std::vector<std::string> L = splitLines(in->pointer, in->size);
		size_t i = 2;
		while (i < L.size()) {
			i = nextSep(L, i);
			if (i >= L.size()) break;
			++i;

			std::string origin = readBlock(L, i);
			std::string target = readBlock(L, i);

			if (origin == target) {
				continue;
			}

			FileMapping map;
			RtlZeroMemory(&map, sizeof(FileMapping));
			map.origin = makeBuffer((const BYTE*)origin.data(), origin.size());
			map.target = makeBuffer((const BYTE*)target.data(), target.size());
			arr.push_back(map);
		}
	}
	out->pointer = toArrayPointer(arr);
	out->size = arr.size();
}

EXTERN_C void FileMappingToBinFile(FileMappingArray* in, ByteBuffer* out) {
	std::vector<BYTE> b;
	size_t n = (in != NULL && in->pointer != NULL) ? in->size : 0;
	appendDWORD(b, BIN_MAGIC);
	appendDWORD(b, FMAP_BIN_MAGIC);
	appendDWORD(b, (DWORD)n);
	for (size_t i = 0; i < n; ++i) {
		const FileMapping& res = in->pointer[i];
		appendDWORD(b, (DWORD)res.origin.size);
		appendDWORD(b, (DWORD)res.target.size);
		appendBytes(b, res.origin.pointer, res.origin.size);
		appendBytes(b, res.target.pointer, res.target.size);
	}
	out->pointer = toArrayPointer(b);
	out->size = b.size();
}

EXTERN_C void BinFileToFileMapping(ByteBuffer* in, FileMappingArray* out) {
	std::vector<FileMapping> arr;
	if (in != NULL && in->pointer != NULL) {
		const BYTE* p = in->pointer;
		const BYTE* end = p + in->size;
		DWORD magic1 = 0, magic2 = 0, count = 0;
		if (!readDWORD(p, end, magic1) || !readDWORD(p, end, magic2) ||
			!readDWORD(p, end, count) || magic1 != BIN_MAGIC || magic2 != FMAP_BIN_MAGIC) {
			count = 0;
		}
		for (DWORD i = 0; i < count; ++i) {
			DWORD os, ts;
			if (!readDWORD(p, end, os) || !readDWORD(p, end, ts)) break;
			if ((size_t)(end - p) < (size_t)(os + ts)) break;
			FileMapping map;
			RtlZeroMemory(&map, sizeof(FileMapping));
			map.origin = makeBuffer(p, os); p += os;
			map.target = makeBuffer(p, ts); p += ts;
			arr.push_back(map);
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

	NameDataToBinFile(in->pName, &buffer);
	appendDWORD(b, (DWORD)buffer.size);
	appendBytes(b, buffer.pointer, buffer.size);
	FreeByteBuffer(&buffer);

	FreeByteBuffer(&buffer);
	PEResourceToBinFile(in->pRes, &buffer);
	appendDWORD(b, (DWORD)buffer.size);
	appendBytes(b, buffer.pointer, buffer.size);

	FreeByteBuffer(&buffer);
	FileMappingToBinFile(in->pFileMap, &buffer);
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
	if (out->pText == NULL) goto fail;
	RtlZeroMemory(out->pText, sizeof(SeenPatchDataArray*) * out->pSize);

	ByteBuffer buffer;
	DWORD size;
	RtlZeroMemory(&buffer, sizeof(ByteBuffer));
	for (size_t i = 0; i < out->pSize; i++) {
		size = 0;
		if (!readDWORD(p, end, size) || size == 0) goto fail;
		buffer.pointer = (BYTE*)p;
		buffer.size = size;
		SeenPatchDataArray* text = (SeenPatchDataArray*)malloc(sizeof(SeenPatchDataArray));
		if (text == NULL) goto fail;
		RtlZeroMemory(text, sizeof(SeenPatchDataArray));
		BinFileToTextData(&buffer, text);
		out->pText[i] = text;
		p += size;
	}

	out->pName = (NameDataArray*)malloc(sizeof(NameDataArray));
	if (out->pName == NULL) goto fail;
	RtlZeroMemory(out->pName, sizeof(NameDataArray));
	if (!readDWORD(p, end, size) || size == 0) goto pe_res;
	buffer.pointer = (BYTE*)p;
	buffer.size = size;
	BinFileToNameData(&buffer, out->pName);
	p += size;

pe_res:
	out->pRes = (PEResourceArray*)malloc(sizeof(PEResourceArray));
	if (out->pRes == NULL) goto fail;
	RtlZeroMemory(out->pRes, sizeof(PEResourceArray));
	if (!readDWORD(p, end, size) || size == 0) goto file_mapping;
	buffer.pointer = (BYTE*)p;
	buffer.size = size;
	BinFileToPEResource(&buffer, out->pRes);
	p += size;

file_mapping:
	out->pFileMap = (FileMappingArray*)malloc(sizeof(FileMappingArray));
	if (out->pFileMap == NULL) goto fail;
	RtlZeroMemory(out->pFileMap, sizeof(FileMappingArray));
	if (!readDWORD(p, end, size) || size == 0) return;
	buffer.pointer = (BYTE*)p;
	buffer.size = size;
	BinFileToFileMapping(&buffer, out->pFileMap);
	return;
fail:
	FreePatchPack(out);
	RtlZeroMemory(out, sizeof(PatchPackStruct));
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

EXTERN_C void FreePEResource(PEResource* data) {
	if (data == NULL) return;
	FreeByteBuffer(&data->origin);
	FreeByteBuffer(&data->translated);
}

EXTERN_C void FreePEResourceArray(PEResourceArray* array) {
	if (array == NULL) return;
	if (array->pointer != NULL) {
		for (size_t i = 0; i < array->size; ++i) {
			FreePEResource(&array->pointer[i]);
		}
		free(array->pointer);
		array->pointer = NULL;
	}
	array->size = 0;
}

EXTERN_C void FreeFileMapping(FileMapping* data) {
	if (data == NULL) return;
	FreeByteBuffer(&data->origin);
	FreeByteBuffer(&data->target);
}

EXTERN_C void FreeFileMappingArray(FileMappingArray* array) {
	if (array == NULL) return;
	if (array->pointer != NULL) {
		for (size_t i = 0; i < array->size; ++i) {
			FreeFileMapping(&array->pointer[i]);
		}
		free(array->pointer);
		array->pointer = NULL;
	}
	array->size = 0;
}

EXTERN_C void FreePatchPack(PatchPack* pack) {
	if (pack->pName != NULL) {
		FreeNameDataArray(pack->pName);
		pack->pName = NULL;
	}
	if (pack->pRes != NULL) {
		FreePEResourceArray(pack->pRes);;
		pack->pRes = NULL;
	}
	if (pack->pFileMap != NULL) {
		FreeFileMappingArray(pack->pFileMap);
		pack->pFileMap = NULL;
	}
	if (pack->pText != NULL && pack->pSize > 0) {
		for (size_t i = 0; i < pack->pSize; i++) {
			FreeSeenPatchDataArray(pack->pText[i]);
		}
	}
	free(pack->pText);
	pack->pText = NULL;
	pack->pSize = 0;
}
