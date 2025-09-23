#include <Windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#define _NO_CVCONST_H
#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")


int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "invalid paramenters" << std::endl;
    }

    std::string output = "symbols";
    if (argc >= 3)
    {
        output = argv[2];
    }

    struct Sym
    {
        ULONG64 Ptr;
        std::string Name;
    };

    static std::vector<Sym> symbols;

    if (!SymInitialize(GetCurrentProcess(), NULL, FALSE)) {
        std::cerr << "Failed to initialize DbgHelp: " << GetLastError() << std::endl;
        return 1;
    }

    DWORD64 baseAddress = SymLoadModuleEx(
        GetCurrentProcess(),
        NULL,
        argv[1],
        NULL,
        0,
        0,
        NULL,
        0
    );

    if (baseAddress == 0) {
        std::cerr << "Failed to load module: " << GetLastError() << std::endl;
        SymCleanup(GetCurrentProcess());
        return 1;
    }

    if (!SymEnumSymbolsEx(GetCurrentProcess(), baseAddress, NULL, +[](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext)->BOOL
    {
        DWORD sym_tag = 0;
        SymGetTypeInfo(GetCurrentProcess(), pSymInfo->ModBase, pSymInfo->TypeIndex, TI_GET_SYMTAG, &sym_tag);
        if (sym_tag == SymTagFunctionType)
        {
            symbols.emplace_back(pSymInfo->Address - pSymInfo->ModBase, pSymInfo->Name);
        }
 
        return TRUE;

    },0, 0)) {
        std::cerr << "Failed to enumerate symbols: " << GetLastError() << std::endl;
        return 1;
    }

    SymCleanup(GetCurrentProcess());

    auto f = std::fstream(output, std::ios::out);


    std::stable_sort(symbols.begin(), symbols.end(), [](auto& a, auto& b){
        return a.Ptr < b.Ptr;
    });

    for (auto& s : symbols)
    {
        f << "0x" << std::hex << s.Ptr << std::dec << " " << s.Name << std::endl;
    }

    std::cout << symbols.size() << " symbols" << std::endl;

    return 0;
}