#pragma once
#include <vector>
#include <string>

extern int ParseSymbolByVA(const std::vector<uint64_t>& addrs, std::vector<struct ModuleInfo>& modules, std::vector<std::string>& symbols);
extern void OpenSymbolFile(const char* path);
