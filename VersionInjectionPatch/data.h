#ifndef __VLSMB_DATA_H
#define __VLSMB_DATA_H

#include <string>
#include <vector>
#include <windef.h>

struct BinFileConverter {
    virtual void loadFromBinFile(const std::vector<BYTE>& bytes) = 0;
    virtual std::vector<BYTE> saveAsBinFile() const = 0;
};

struct TextFileConverter {
    virtual void loadFromTextFile(const std::string& text) = 0;
};

struct SeenTextItem {
    unsigned index;
    unsigned offset;
    size_t length;
    bool replaceFlag;
    std::string translated;
};

struct SeenTextData : BinFileConverter, TextFileConverter {
    unsigned seenNo;
    std::vector<SeenTextItem> items;

    void loadFromBinFile(const std::vector<BYTE>& bytes) override;
    std::vector<BYTE> saveAsBinFile() const override;
    void loadFromTextFile(const std::string& text) override;
};

struct NameItem {
    std::string origin;
    std::string translated;
};

struct NameData : BinFileConverter, TextFileConverter {
    std::vector<NameItem> data;

    void loadFromBinFile(const std::vector<BYTE>& bytes) override;
    std::vector<BYTE> saveAsBinFile() const override;
    void loadFromTextFile(const std::string& text) override;
};

enum class PEResourceEncode : DWORD {
    ENCODE_UNKNOWN, GBK, UTF16LE
};

enum class PEResourceStrategy : DWORD {
    STRATEGY_UNKNOWN, FILL_NULL, FILL_SPACE
};

struct PEResourceItem {
    unsigned index;
    DWORD rva;
    size_t length;
    std::string translated;
    PEResourceEncode encode;
    PEResourceStrategy strategy;
};

struct PEResourceData : BinFileConverter, TextFileConverter {
    std::vector<PEResourceItem> data;

    void loadFromBinFile(const std::vector<BYTE>& bytes) override;
    std::vector<BYTE> saveAsBinFile() const override;
    void loadFromTextFile(const std::string& text) override;
};

struct FileMappingItem {
    std::string origin;
    std::string target;
};

struct FileMappringData : BinFileConverter, TextFileConverter {
    std::vector<FileMappingItem> data;

    void loadFromBinFile(const std::vector<BYTE>& bytes) override;
    std::vector<BYTE> saveAsBinFile() const override;
    void loadFromTextFile(const std::string& text) override;
};

struct PatchPack : BinFileConverter {
    std::vector<SeenTextData> text;
    NameData name;
    PEResourceData res;
    FileMappringData fileMap;

    void loadFromBinFile(const std::vector<BYTE>& bytes) override;
    std::vector<BYTE> saveAsBinFile() const override;
    void update(PatchPack&& other);
};

#endif
