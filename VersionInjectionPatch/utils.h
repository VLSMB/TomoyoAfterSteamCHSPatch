#ifndef __VLSMB_UTILS_H
#define __VLSMB_UTILS_H

#include "define.h"
#include <vector>
#include <string>

void appendDWORD(std::vector<BYTE>& b, DWORD v);
void appendString(std::vector<BYTE>& b, const std::string& text);
bool readDWORD(const std::vector<BYTE>& buf, size_t& offset, DWORD& v);
std::vector<std::string> splitLines(const std::string& p);
bool isSep(const std::string& s);
size_t nextSep(const std::vector<std::string>& L, size_t i);
std::string readBlock(const std::vector<std::string>& L, size_t& i);
std::string transferToGbk(const std::string& str);
bool startsWith(const std::string& str, const std::string& prefix);
bool GbkToUtf16Le(const std::string& gbk, std::string& out);

#endif
