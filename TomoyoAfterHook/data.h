#ifndef __VLSMB_DATA_H
#define __VLSMB_DATA_H

#include <Windows.h>

#define SEEN_DATA_NUM 10000

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct RealLiveVMStateStruct {
    unsigned seenNo;
} RealLiveVMState;

typedef struct RealLiveVMContextStruct {
    DWORD unknown1;
    DWORD unknown2;
    DWORD unknown3;
    DWORD unknown4;
    DWORD unknown5;
    DWORD unknown6;
    BYTE* vmBase;
    DWORD unknown7;
    BYTE* vmIp;
} RealLiveVMContext;

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

typedef struct SeenPatchDataStruct {
	unsigned index;
	unsigned offset;
    size_t length;
    ByteBuffer name;
    ByteBuffer origin;
    ByteBuffer translated;
} SeenPatchData;

typedef struct SeenPatchDataArrayStruct {
    unsigned seenNo;
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

typedef struct SeenDumpDataStruct {
    SeenPatchDataArray textData;
    NameDataArray nameData;
} SeenDumpData;

typedef struct PatchPackStruct {
    SeenPatchDataArray** pText;
    size_t pSize;
    NameDataArray* pName;
} PatchPack;

typedef struct CharacterInfoStruct {
    unsigned seenNo;
    unsigned offset;
    unsigned length;
    BYTE asciiFlag;
    BYTE lastFlag;
    WORD character;
} CharacterInfo;

void DumpSeenData(RealLiveSeenData* in, SeenDumpData* out);

void TextDataToTextFile(SeenPatchDataArray* in, ByteBuffer* out);
void TextFileToTextData(ByteBuffer* in, SeenPatchDataArray* out);
void TextDataToBinFile(SeenPatchDataArray* in, ByteBuffer* out);
void BinFileToTextData(ByteBuffer* in, SeenPatchDataArray* out);

void NameDataToTextFile(NameDataArray* in, ByteBuffer* out);
void TextFileToNameData(ByteBuffer* in, NameDataArray* out);
void NameDataToBinFile(NameDataArray* in, ByteBuffer* out);
void BinFileToNameData(ByteBuffer* in, NameDataArray* out);

void PatchPackToBinFile(PatchPack* in, ByteBuffer* out);
void BinFileToPatchPack(ByteBuffer* in, PatchPack* out);

NameDataArray* MergeNameDataArray(NameDataArray** arrayList, size_t arraySize);
BOOL UpdateSeenPatchDataArray(SeenPatchDataArray** fromDll, size_t szDll, SeenPatchDataArray** fromLocal, size_t szLocal, SeenPatchDataArray*** out, size_t* szOut);

void FreeByteBuffer(ByteBuffer* buf);
void FreeSeenPatchData(SeenPatchData* data);
void FreeSeenPatchDataArray(SeenPatchDataArray* array);
void FreeNameData(NameData* data);
void FreeNameDataArray(NameDataArray* array);
void FreeSeenDumpData(SeenDumpData* dump);
void FreePatchPack(PatchPack* pack);

BOOL InitPatchData(PatchPack* in);
void CleanPatchData();
const char* const GetTranslatedName(const char* const name);
const char* const GetTranslatedText(const char* const text);
void UpdateSeenBuffer(BYTE* buffer, unsigned seenNo);
BOOL GetNextCharacterInfo(unsigned seenNo, unsigned offset, CharacterInfo* out);
void AckConsumeCharacter(const CharacterInfo* out);

#ifdef __cplusplus
}
#endif
#endif
