#ifndef __VLSMB_PATCH_H
#define __VLSMB_PATCH_H

#include "define.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL InitPatchData(PatchPack* in);
void CleanPatchData();
const char* const GetTranslatedName(const char* const name);
const char* const GetTranslatedText(const char* const text);
void SetTranslatedPEResource(void* imageBase);
void UpdateSeenBuffer(BYTE* buffer, unsigned seenNo);
BOOL GetNextCharacterInfo(unsigned seenNo, unsigned offset, CharacterInfo* out);
void AckConsumeCharacter(const CharacterInfo* out);

BOOL IsWindowTitleText(const char* const text);
const char* const GetWindowTitleTranslatedText(const char* const text);
const char* const ApplyFilePathMapping(const char* const text);

const char* const CheckTextForSeen9900(const char* const text);
void CheckG00ForSeen9900(const char* const file);
BOOL IsInSeen9900Mode();
void PaintCurFrameInSeen9900Mode(HWND hwnd, HDC hdc);

#ifdef __cplusplus
}
#endif

#endif
