#ifndef __VLSMB_DATA_H
#define __VLSMB_DATA_H

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PatchModeEnum {
    PATCH_RELEASE,
    PATCH_DUMP,
    PATCH_ARCHIVE,
    PATCH_DEBUG,
    PATCH_NONE
} PatchMode;

typedef struct ByteBufferStruct {
    BYTE* pointer;
    size_t size;
} ByteBuffer;

typedef struct RealLiveSeenHeaderStruct {
    DWORD offset;
    DWORD size;
} RealLiveSeenHeader;

typedef struct RealLiveSeenDataStruct {
    DWORD unknown1;
    DWORD unknown2;
    DWORD unknown3;
    DWORD unknown4;
    DWORD unknown5;
    DWORD unknown6;
    BYTE* decompressed_data;
    DWORD decompressed_size;
} RealLiveSeenData;

typedef struct SeenPatchDataStruct {
	unsigned number;
	unsigned offset;
    ByteBuffer name;
    ByteBuffer origin;
    ByteBuffer translated;
} SeenPatchData;

typedef struct SeenPatchDataArrayStruct {
    SeenPatchData* pointer;
    size_t size;
} SeenPatchDataArray;

typedef struct NameDataStruct {
    ByteBuffer origin;
    ByteBuffer translated;
} NameData;

typedef struct NameDataArrayStruct {
    NameData* pointer;
    size_t size;
} NameDataArray;

typedef struct SingleWordExtendMapStruct {
    WORD word;
    ByteBuffer sentence;
} SingleWordExtendMap;

typedef struct SeenDumpDataStruct {
	unsigned number;
    SeenPatchDataArray textData;
    NameDataArray nameData;
} SeenDumpData;

void DumpSeenData(unsigned seenNo, RealLiveSeenData* in, SeenDumpData* out);

void TextDataToTextFile(SeenPatchDataArray* in, ByteBuffer* out);
void TextFileToTextData(ByteBuffer* in, SeenPatchDataArray* out);
void TextDataToBinFile(SeenPatchDataArray* in, ByteBuffer* out);
void BinFileToTextData(ByteBuffer* in, SeenPatchDataArray* out);

void NameDataToTextFile(NameDataArray* in, ByteBuffer* out);
void TextFileToNameData(ByteBuffer* in, NameDataArray* out);
void NameDataToBinFile(NameDataArray* in, ByteBuffer* out);
void BinFileToNameData(ByteBuffer* in, NameDataArray* out);

NameDataArray* MergeNameDataArray(NameDataArray** arrayList, size_t arraySize);

void FreeByteBuffer(ByteBuffer* buf);
void FreeSeenPatchData(SeenPatchData* data);
void FreeSeenPatchDataArray(SeenPatchDataArray* array);
void FreeNameData(NameData* data);
void FreeNameDataArray(NameDataArray* array);
void FreeSingleWordExtendMap(SingleWordExtendMap* map);
void FreeSeenDumpData(SeenDumpData* dump);

#ifdef __cplusplus
}
#endif
#endif
