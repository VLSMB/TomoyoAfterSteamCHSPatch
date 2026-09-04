#ifndef __VLSMB_DATA_H
#define __VLSMB_DATA_H

#include "struct.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
#endif
