#include "data.h"
#include <vector>
#include <set>
#include <utility>

template<typename T>
T* toArrayPointer(const std::vector<T>& vec);

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
            if (b == '"') {
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

void TextDataToTextFile(SeenPatchDataArray* in, ByteBuffer* out) {

}

void TextFileToTextData(ByteBuffer* in, SeenPatchDataArray* out) {

}

void TextDataToBinFile(SeenPatchDataArray* in, ByteBuffer* out) {

}

void BinFileToTextData(ByteBuffer* in, SeenPatchDataArray* out) {

}

void NameDataToTextFile(NameDataArray* in, ByteBuffer* out) {

}

void TextFileToNameData(ByteBuffer* in, NameDataArray* out) {

}

void NameDataToBinFile(NameDataArray* in, ByteBuffer* out) {

}

void BinFileToNameData(ByteBuffer* in, NameDataArray* out) {

}

NameDataArray* mergeNameDataArray(NameDataArray** arrayList, size_t arraySize) {

}

template<typename T>
T* toArrayPointer(const std::vector<T>& vec) {
    static_assert(std::is_trivially_copyable_v<T>, "¿‡–Õ¥ÌŒÛ");
    T* arr = (T*)malloc(vec.size() * sizeof(T));
    if (arr == NULL) {
        ExitProcess(1);
    }
    if (!vec.empty()) {
        memcpy(arr, vec.data(), vec.size() * sizeof(T));
    }
    return arr;
}
