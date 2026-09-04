#ifndef __VLSMB_PATCH_H
#define __VLSMB_PATCH_H

#include "struct.h"
#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL InitPatchData(PatchPack* in);
void CleanPatchData();
const char* const GetTranslatedName(const char* const name);
const char* const GetTranslatedText(const char* const text);
void UpdateSeenBuffer(BYTE* buffer, unsigned seenNo);
BOOL GetNextCharacterInfo(unsigned seenNo, unsigned offset, CharacterInfo* out);
void AckConsumeCharacter(const CharacterInfo* out);

BOOL IsWindowTitleText(const char* const text);
const char* const GetWindowTitleTranslatedText(const char* const text);

#ifdef __cplusplus
}
#endif

#endif
