
#include <iostream>
#include <map>
#include <unordered_map>
#include <memory>
#include <locale>
#include <codecvt>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <format>
#include <future>
#include <span>
#include <ranges>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "MemoryTraceParser.h"
#include "demangle.h"



std::string run_cmd(const std::string& cmd)
{
#if PLATFORM_WINDOWS
	char   psBuffer[1024];
	FILE* pPipe;

	if ((pPipe = _popen(cmd.c_str(), "rb")) == NULL)
		return {};

	std::string content;
	size_t count;
	while (count = fread(psBuffer, 1, sizeof(psBuffer), pPipe)) 
	{
		content += std::string(psBuffer, count);
	}


	if (feof(pPipe))
		_pclose(pPipe);

	return content;
#else
	auto pipe = popen(cmd.c_str(), "r");
	if (!pipe)
		return {};

	std::string str;
	char buffer[1024];
	size_t count;
	while (count = fread(buffer, 1, 1024, pipe))
	{
		str += std::string(buffer, count);
	}

	pclose(pipe);
	return str;
#endif
};


static bool is_parsing_android = false;

struct Symbol
{
	uint64_t offset;
	std::string name;
};
static std::vector<Symbol> so_symbols;
bool OpenSOFile(const char* path)
{
	std::string path_str = path;
	if (path_str.find(".so") == std::string::npos)
	{
		return false;
	}


	auto content = run_cmd(std::format("nm -DC {0}", path));
	auto size = content.size();
	size_t word = 0;
	size_t str_len = 0;
	bool skip = false;
	std::string_view addr;

	std::vector<Symbol> syms;
    char attr;
    int state = 0;
	for (size_t i = 0; i < size; ++i)
	{
		auto chara = content[i];
		if (chara == '\n')
		{
            if (!skip)
            {
                std::string_view name = { &content[word], str_len };
                
                Symbol s = { std::stoull(std::string(addr), nullptr, 16), std::string(name) };
                syms.push_back(s);
            }

			str_len = 0;
			skip = false;
			word = i + 1;
            state = 0;
		}
		else if (chara == ' ' && state < 2)
		{
			if (str_len > 1)
			{
				addr = { &content[word], str_len };
                str_len = 0;
                word = i + 1;
                state = 1;
			}
			else if (str_len == 0)
			{
				word = i;
			}
			else if (str_len == 1)
			{
                attr = content[word];
				if (attr == 'U' )
					skip = true;
                
                word = i + 1;
                str_len = 0;
                state = 2;
			}

		}
		else
		{
			str_len++;
		}
	}


	std::stable_sort(syms.begin(), syms.end(), [](auto& a, auto& b){
		return a.offset < b.offset;
	});
	so_symbols = std::move(syms);
    
    is_parsing_android = true;
	return true;
}

void ParseSymbolBySO(const std::vector<uint64_t>& addrs , std::vector<ModuleInfo>& modules,  std::vector<std::string>& symbols)
{
    symbols.reserve(addrs.size());
    
    
    uint64_t base_addr = 0;
    for (auto& info : modules)
    {
        if (info.name.find("libUE4") != std::string::npos)
        {
            base_addr = info.base_addr;
        }
    }
    for (auto addr : addrs)
    {
        if (addr > base_addr)
        {
            auto pos = std::upper_bound(so_symbols.begin(), so_symbols.end(), addr - base_addr, [](auto& a, auto& b){
                return  a < b.offset;
            });
            
            if (pos == so_symbols.end())
            {
                symbols.push_back(std::to_string(addr));
            }
            else{
                symbols.push_back(std::string((pos - 1)->name));
            }
        }
        else{
            symbols.push_back(std::to_string(addr));
        }
    }
}

static std::string getFileName(std::string path)
{
	std::replace(path.begin(), path.end(), '/', '\\');
	auto name_pos = path.find_last_of('\\');
	return path.substr(name_pos + 1);
}

static std::vector<Symbol> symbols_parser(const std::string& content)
{
    auto hex_to_uint64_manual = [](const char* start, const char* end)
    {
        uint64_t result = 0;
        
        while(start < end) {
            result <<= 4;  
            
            if(*start >= '0' && *start <= '9') {
                result += (*start - '0');
            }
            else if(*start >= 'a' && *start <= 'f') {
                result += (*start - 'a' + 10);
            }
            else if(*start >= 'A' && *start <= 'F') {
                result += (*start - 'A' + 10);
            }
            
            start++;
        }
        
        return result;
    };
    
    const char* begin = content.data();
    const char* end = begin + content.size();
    std::vector<Symbol> syms;
    syms.reserve(1000000);
    
    const char* start = begin;
    uint64_t addr = 0;
    std::string name;
    int state = 0;
    while(begin <= end)
    {
        if (state == 1)
        {
            if (*begin == '\n' || begin == end)
            {
                name = std::string(start, begin - start);
                syms.push_back({addr, name});
                start = begin + 1;
                state = 0;
            }
        }
        else
        {
            if (*begin == ' ')
            {
                addr = hex_to_uint64_manual(start + 2, begin);
                start = begin + 1;
                state = 1;
            }
        }
    
        begin++;
    }
    
    Assert(start >= end, "symbol parsing is failed.");
    
	std::stable_sort(syms.begin(), syms.end(), [](auto& a, auto& b) {
		return a.offset < b.offset;
	});

    auto base_addr = syms[0].offset;

	if ((base_addr & ~(uint64_t)0xffffffff) == 0)
	{
		base_addr = 0;
	}
    else
	{
		for(auto& sym : syms)
		{
			sym.offset -= base_addr;
		}
	}
    

    
    return syms;
}

//static std::vector<Symbol> dsym_parser(const std::string& content);
static std::unordered_map<std::string, std::vector<Symbol>> symbol_map;
//std::vector<Symbol> loaded_symbols;
void OpenSymbolFile(const char* path, std::string module_name)
{
	if (OpenSOFile(path))
	{
        
	}
	else
	{
        
		std::string path_str = path;

		std::ifstream f(path,std::ios::binary);
		if (!f)
			return;
		auto size = std::filesystem::file_size(path);
		std::string content;
		content.resize(size);
		f.read(content.data(), content.size());

        //if (using_dsym)
        //    symbol_map["ProjectZClient"] = dsym_parser(std::move(content));
        //else
            symbol_map[module_name] = symbols_parser(std::move(content));
	}
}

static std::string convert_symbol_name(const char* name)
{
	char temp[1024];
	if (demangle(temp, 1024, name + 1))
	{
		return temp;
	}
	return name;
}

static std::vector<std::string> addr2symbol(const std::vector<Symbol>& symbols, uint64_t base, const std::vector<uint64_t>& addrs)
{
    std::vector<std::string> result;
    result.reserve(addrs.size());
    auto search_begin = symbols.begin();
    auto search_end = symbols.end();

    for (auto addr : addrs)
    {
        auto va = addr - base;
        if (va >= symbols[0].offset)
        {

            search_begin = std::upper_bound(search_begin, search_end,va, [](auto& a, auto& b){
                return a < b.offset;
            });
            Assert(search_begin != search_end, "failed to find symbol");
            search_begin--;

            result.push_back(search_begin->name);
        }
        else
        {
            result.push_back("unknown");
        }
    }

    return result;
}

#define ALIGN(x,a)              __ALIGN_MASK(x,a - 1)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))


int ParseSymbolByVA(const std::vector<uint64_t>& addrs, std::vector<ModuleInfo>& modules, std::vector<std::string>& symbols)
{
    if (is_parsing_android)
    {
        ParseSymbolBySO(addrs,modules, symbols);
        return symbols.size();
    }
    
    
    struct InternalModuleInfo: public ModuleInfo
    {
        std::vector<Symbol>* symbols = nullptr;
    };
    
    
    std::sort(modules.begin(), modules.end(), [](auto a, auto b) {
        return a.base_addr < b.base_addr;
    });


    std::vector<InternalModuleInfo > moduleInfos;
    moduleInfos.reserve((modules.size()));
    for (auto& m : modules)
    {
        auto dsym = symbol_map.find(m.name);
        
        {
            InternalModuleInfo mi;
            mi.base_addr = m.base_addr;
            mi.size = m.size;
            mi.name = m.name;
            if (dsym != symbol_map.end())
                mi.symbols = &dsym->second;
            moduleInfos.push_back(std::move(mi));
        }
    }

    struct AddrInfo
    {
        uint64_t addr;
        uint32_t pos;
    };
    std::unordered_map<InternalModuleInfo*, std::vector<AddrInfo>> sorted_addrs;
    symbols.resize(addrs.size(), {});
    uint32_t cur = 0;
    for (auto addr: addrs)
    {
        auto mi = std::upper_bound(moduleInfos.begin(), moduleInfos.end(), addr, [](auto& a, auto& b) {
            return a < b.base_addr;
        });

        if (mi != moduleInfos.begin())
        {
            --mi;
            if (mi->symbols == 0)
            {
                symbols[cur++] = std::format("(in {0})",mi->name );
                continue;
            }
            else if (mi->base_addr + mi->size >= addr)
            {
                sorted_addrs[&(*mi)].push_back({addr, cur++});
                continue;
            }
        }
        

        {
            symbols[cur++] = std::format("{0:#x}", addr);
        }
    }
    


    const int task_size = 128;
    const int num_task_group = sorted_addrs.size();
    
    

	std::vector<std::future<bool>> workers;

    struct TaskInfo
    {
        InternalModuleInfo* module_info;
        std::span<AddrInfo> group;
    };
    
    std::vector<TaskInfo> task_list;
    for (auto& item: sorted_addrs)
    {
        auto addr_list = std::span(item.second);
        task_list.push_back({item.first, addr_list.subspan(0, addr_list.size())});

    }
    
    std::mutex mutex;
    
    
	const auto parse_sym = [&](){
        while(true)
        {
            TaskInfo task;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (task_list.empty())
                    return true;
                task = *task_list.rbegin();
                task_list.pop_back();
            }
            std::vector<uint64_t> list;
            list.reserve(task.group.size());
            for (auto& info: task.group)
            {
                list.push_back(info.addr);
            }
            auto result = addr2symbol(*task.module_info->symbols, task.module_info->base_addr, list);
            
            for (int i = 0; i < result.size(); ++i)
            {
                symbols[task.group[i].pos] = result[i] ;
            }
            
        }
		return true;
	};

	for (int i = 0; i < num_task_group; ++i)
	{
		workers.push_back(std::async(parse_sym));
	}

	for (auto& f: workers)
	{
		f.get();
	}

	return addrs.size();
}

