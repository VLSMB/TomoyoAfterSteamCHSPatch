#ifndef __VLSMB_PATCH_H
#define __VLSMB_PATCH_H

#include "data.h"
#include <unordered_map>
#include <optional>
#include <stack>
#include <memory>

template<typename T>
using seen_map = std::unordered_map<unsigned, std::unordered_map<unsigned, T>>;

struct CharacterInfo {
    unsigned seenNo{};
    unsigned offset{};
    unsigned length{};
    BYTE asciiFlag{};
    BYTE lastFlag{};
    WORD character{};
};

class Seen9900Flag {
private:
    bool typeEnFlag{};
    bool type999Flag{};
    int index{};
    const std::vector<std::string> texts{};
public:
    Seen9900Flag();
    explicit Seen9900Flag(std::vector<std::string>&& t);
    void reset();
    void setIndex(size_t idx);
    void setTypeEn();
    void setType999();
    bool isFlag() const;
    std::string curText() const;
};

struct SeenTextRT : SeenTextItem {
    std::stack<BYTE> text_stack;
};

class PatchRT {
private:
    Seen9900Flag seen9900_flag{};
    std::unordered_map<std::string, NameItem> name_map{};
    seen_map<SeenTextRT> seen_map{};
    std::vector<PEResourceItem> pe_res{};
    std::unordered_map<std::string, FileMappingItem> file_mapping{};
public:

    explicit PatchRT(PatchPack&& pack);
    PatchRT(const PatchRT&) = delete;
    PatchRT& operator=(const PatchRT&) = delete;
    PatchRT(PatchRT&&) = delete;
    PatchRT& operator=(PatchRT&&) = delete;
    ~PatchRT() = default;

    std::optional<std::reference_wrapper<const std::string>>
        getTranslatedName(const std::string& name) const;

    std::optional<std::reference_wrapper<const SeenTextItem>> 
        getTranslatedText(unsigned seenNo, unsigned offset) const;

    std::optional<std::reference_wrapper<const std::string>>
        applyFilePathMapping(const std::string& path) const;

    void setTranslatedPERes(void* imageBase) const;
    void updateSeenBuffer(BYTE* buffer, unsigned seenNo) const;
    bool getNextCharacterInfo(unsigned seenNo, unsigned offset, CharacterInfo& info) const;
    void ackConsumeCharacter(const CharacterInfo& info);

    std::optional<std::reference_wrapper<const std::string>>
        checkTextForSeen9900(const std::string& text);

    void checkG00ForSeen9900(const std::string& file);
    bool isInSeen9900Mode() const;
    void paintCurFrameInSeen9900Mode(HWND hwnd, HDC hdc);
};

#endif
