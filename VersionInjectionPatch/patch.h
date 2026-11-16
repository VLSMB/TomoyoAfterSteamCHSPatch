#ifndef __VLSMB_PATCH_H
#define __VLSMB_PATCH_H

#include "data.h"
#include <unordered_map>
#include <optional>
#include <stack>
#include <memory>

struct CharacterInfo {
    unsigned seenNo{};
    unsigned offset{};
    unsigned length{};
    bool asciiFlag{};
    bool lastFlag{};
    WORD character{};
};

class Seen9900Flag {
private:
    bool typeEnFlag{};
    bool type999Flag{};
    unsigned offset{};
public:
    void reset();
    void setOffset(unsigned off);
    void setTypeEn();
    void setType999();
    bool isFlag() const;
    unsigned getOffset() const;
};

struct SeenTextRT : SeenTextItem {
    bool inStack{};
    std::stack<BYTE> text_stack{};

    explicit SeenTextRT(SeenTextItem&&);
    SeenTextRT() = default;
    ~SeenTextRT() = default;
    SeenTextRT(SeenTextRT&&) = default;
    SeenTextRT(const SeenTextRT&) = delete;
    SeenTextRT& operator=(SeenTextRT&&) = default;
    SeenTextRT& operator=(const SeenTextRT&) = delete;
};

class PatchRT {
private:
    std::unordered_map<std::string, NameItem> name_map{};
    std::unordered_map<unsigned, std::unordered_map<unsigned, SeenTextRT>> seen_map{};
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
    bool getNextCharacterInfo(unsigned seenNo, unsigned offset, CharacterInfo& info);
    void ackConsumeCharacter(const CharacterInfo& info);

    void checkG00ForSeen9900(const std::string& file);
    bool isInSeen9900Mode() const;
    void paintCurFrameInSeen9900Mode(HWND hwnd, HDC hdc);
};

#endif
