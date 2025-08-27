
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

#if WIN64_PARSER

#include "DIA/include/dia2.h"
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "DIA\\lib\\x64\\diaguids.lib")



static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;



class PDBFile
{
public:
	bool load(const char* path)
	{
		if (!path) return false;
		if (strlen(path) == 0) return false;

		file_name = getFileName(path);
		file_name.erase(file_name.length() - 4);

		HRESULT hr = ::CoCreateInstance(CLSID_DiaSource, 0, CLSCTX_INPROC_SERVER, IID_IDiaDataSource, (void**)&DataSource);
		if (FAILED(hr))
			return false;


		std::wstring wpath = converter.from_bytes(path);
		hr = DataSource->loadDataFromPdb(wpath.c_str());

		if (FAILED(hr))
		{
			switch (hr)
			{
			case E_PDB_NOT_FOUND:
			case E_PDB_FORMAT:
			case E_INVALIDARG:
				return false;
			case E_UNEXPECTED:
				break;
			default:
				return false;
			}
		}

		DataSource->openSession(&Session) == S_OK ? true : false;


		if (FAILED(Session->get_globalScope(&Symbol)))
			return false;

		isStripped = false;
		BOOL b;
		if (SUCCEEDED(Symbol->get_isStripped(&b)))
		{
			isStripped = b;
		}

		return true;
	}

	bool findSymbol(uint64_t va, std::string& si)
	{
		IDiaSymbol* sym = nullptr;
		Session->findSymbolByVA(va, SymTagFunction, &sym);
		if (!sym)
		{
			Session->findSymbolByVA(va, SymTagPublicSymbol, &sym);
		}

		if (!sym)
			return false;

		BSTR name = nullptr;
		sym->get_name(&name);
		std::string sym_name = getFileName(converter.to_bytes(name));
		si = sym_name;
		//IDiaEnumLineNumbers* lineEnum = nullptr;
		//Session->findLinesByVA(va, 1, &lineEnum);

		//ULONG celt = 0;
		//DWORD lineNo = 0;
		//BSTR file = nullptr;
		//while (lineEnum)
		//{
		//	IDiaLineNumber* line = NULL;
		//	lineEnum->Next(1, &line, &celt);
		//	if (!line)
		//		celt = 1;			// hack, no file and line but has symbol name

		//	if (celt == 1)
		//	{
		//		IDiaSourceFile* src = NULL;

		//		if (line)
		//		{
		//			line->get_sourceFile(&src);
		//			line->get_lineNumber(&lineNo);
		//		}
		//		if (src)
		//			src->get_fileName(&file);




		//		if (file)
		//		{
		//			si = std::format("{0} ({1}:{2})",sym_name, getFileName(converter.to_bytes(file)), lineNo);
		//		}
		//		else
		//			si = sym_name;

		//		SysFreeString(name);

		//		if (file)
		//			SysFreeString(file);

		//		if (line)
		//			line->Release();

		//		if (src)
		//			src->Release();

		//		break;
		//	}
		//	if (celt != 1)
		//		break;
		//}


		//lineEnum->Release();
		sym->Release();

		return true;
	}

	~PDBFile()
	{
		if (DataSource) DataSource->Release();
		if (Session) Session->Release();
		if (Symbol) Symbol->Release();

	}

	const std::string& GetName() { return file_name; }
private:
	std::string file_name;
	IDiaDataSource* DataSource = nullptr;
	IDiaSession* Session = nullptr;
	IDiaSymbol* Symbol = nullptr;
	bool isStripped = false;
};


static std::map<std::string, std::shared_ptr<PDBFile>> PDBFilesMap;



class Env
{
public:
	Env()
	{
		auto hr = ::CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY);
		Assert(SUCCEEDED(hr) , "failed to call coinitialze");

	}
	~Env()
	{
		PDBFilesMap.clear();
		::CoUninitialize();
	}
}EnvVar;


void OpenSymbolFile(const char* path)
{

	if (OpenSOFile(path))
	{

	}
	else
	{
		auto pdb = std::make_shared<PDBFile>();
		if (!pdb->load(path))
			return ;

		if (PDBFilesMap.find(pdb->GetName()) != PDBFilesMap.end())
		{
			return ;
		}
	
		PDBFilesMap.emplace(pdb->GetName(), pdb);
	}
}


int ParseSymbolByVA(const std::vector<uint64_t>& addrs , std::vector<ModuleInfo>& modules, std::vector<std::string>& symbols)
{

	if (modules .empty()|| addrs.empty())
	{
		return 0;
	}


	struct InternalModuleInfo: public ModuleInfo
	{
		std::shared_ptr<PDBFile> pdb;
	};


	std::sort(modules.begin(), modules.end(), [](auto a, auto b) {
		return a.base_addr < b.base_addr;
	});


	std::vector<InternalModuleInfo > moduleInfos;
	moduleInfos.reserve((modules.size()));
	for (auto& m : modules)
	{
		auto pdb_name = m.name;
		auto pdb = PDBFilesMap.find(pdb_name);
		if (pdb != PDBFilesMap.end())
		{
			InternalModuleInfo mi;
			mi.base_addr = m.base_addr;
			mi.size = m.size;
			mi.name = m.name;
			mi.pdb = pdb->second;
			moduleInfos.push_back(std::move(mi));
		}
	}



	int parsedcount = 0;
	for (auto& addr : addrs)
	{

		auto mi = std::upper_bound(moduleInfos.begin(), moduleInfos.end(), addr, [](auto a, auto b) {
			return a < b.base_addr;
		});

		symbols.push_back({});
		auto& sym = *symbols.rbegin();
		if (mi != moduleInfos.begin())
		{
			--mi;

			if (mi->base_addr + mi->size >= addr)
			{
				auto va = addr - mi->base_addr;
				
				if (!mi->pdb || !mi->pdb->findSymbol(va, sym))
				{
					sym = std::format("{0} {1: #x}", mi->name, addr);
				}
				continue;
			}
		}

		{
			sym = std::format("{0:#x}", addr);
		}

		++parsedcount;
	}


	return parsedcount;
}

#elif IOS_PARSER
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

static std::vector<Symbol> dsym_parser(const std::string& content);
static std::unordered_map<std::string, std::vector<Symbol>> symbol_map;
void OpenSymbolFile(const char* path)
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
        
        
        bool using_dsym = getFileName(path_str) == "ProjectZClient";

        if (using_dsym)
            symbol_map["ProjectZClient"] = dsym_parser(std::move(content));
        else
            symbol_map["ProjectZClient"] = symbols_parser(std::move(content));
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


static std::vector<Symbol> dsym_parser(const std::string& content)
{
#define	MH_MAGIC_64	0xfeedfacf	/* the mach magic number */
#define MH_DSYM        0xa
#define	LC_SYMTAB	0x2	/* link-edit stab symbol table info */
#define LC_UUID		0x1b	/* the uuid */
#define	LC_SEGMENT_64	0x19	/* 64-bit segment of this file to be mapped */

	struct mach_header_64 {
		uint32_t	magic;		/* mach magic number identifier */
		int	cputype;	/* cpu specifier */
		int	cpusubtype;	/* machine specifier */
		uint32_t	filetype;	/* type of file */
		uint32_t	ncmds;		/* number of load commands */
		uint32_t	sizeofcmds;	/* the size of all the load commands */
		uint32_t	flags;		/* flags */
		uint32_t	reserved;	/* reserved */
	};

	struct load_command {
		uint32_t cmd;
		uint32_t cmdsize;
	};

	struct segment_command_64 { /* for 64-bit architectures */
		char		segname[16];	/* segment name */
		uint64_t	vmaddr;		/* memory address of this segment */
		uint64_t	vmsize;		/* memory size of this segment */
		uint64_t	fileoff;	/* file offset of this segment */
		uint64_t	filesize;	/* amount to map from the file */
		int	maxprot;	/* maximum VM protection */
		int	initprot;	/* initial VM protection */
		uint32_t	nsects;		/* number of sections in segment */
		uint32_t	flags;		/* flags */
	};

	struct section_64 {          /* for 64-bit architectures */
		char       sectname[16];  /* name of this section */
		char       segname[16];   /* segment this section goes in */
		uint64_t   addr;          /* memory address of this section */
		uint64_t   size;          /* size in bytes of this section */
		uint32_t   offset;        /* file offset of this section */
		uint32_t   align;         /* section alignment (power of 2) */
		uint32_t   reloff;        /* file offset of relocation entries */
		uint32_t   nreloc;        /* number of relocation entries */
		uint32_t   flags;         /* flags (section type and attributes)*/
		uint32_t   reserved1;     /* reserved (for offset or index) */
		uint32_t   reserved2;     /* reserved (for count or sizeof) */
		uint32_t   reserved3;     /* reserved */
	};

	struct symtab_command {
		uint32_t symoff;
		uint32_t nsyms;
		uint32_t stroff;
		uint32_t strsize;
	};

	struct uuid_command
	{
		uint8_t uuid[16];
	};


	struct nlist_64 {
		uint32_t n_strx;
		uint8_t n_type;
		uint8_t n_sect;
		uint16_t n_desc;
		uint64_t n_value;
	};

	//auto file_name = dsym;
	//std::ifstream file(file_name, std::ios::binary);
	//if (!file) {
	//	return {};
	//}
	//auto size = std::filesystem::file_size(file_name);


	//std::string content;
	//content.resize(size, 0);
	//file.read(content.data(), size);
	const char* begin = content.data();
	const char* cur = begin;
	const char* end = content.data() + content.size();
	auto read = [&](auto& value) {
		memcpy(&value, cur, sizeof(value));
		cur += sizeof(value);
		assert(cur <= end);
	};

	auto read_from = [](auto& value, const char*& ptr) {
		memcpy(&value, ptr, sizeof(value));
		ptr += sizeof(value);
	};

	auto skip = [&](int num) {
		cur += num;
		assert(cur <= end);
	};

	mach_header_64 header;
	read(header);

	Assert(header.magic == MH_MAGIC_64, "invalid symbol file");
	Assert(header.filetype == MH_DSYM, "invalid symbol file");


	std::vector<Symbol> vas;
	std::vector<segment_command_64> segs;
	vas.reserve(1024 * 1024);
	uint64_t base_addr = 0;
	uint64_t code_size = 0;

	for (int i = 0; i < header.ncmds ; ++i)
	{
		load_command cmd;
		read(cmd);

		if (cmd.cmd == LC_SEGMENT_64)
		{
			segment_command_64 seg;
			read(seg);
			segs.push_back(seg);
			if (strcmp(seg.segname, "__TEXT") == 0)
			{
				base_addr = seg.vmaddr;
				code_size = seg.vmsize;
			}
			skip(sizeof(section_64) * seg.nsects);
		}
		else if (cmd.cmd == LC_SYMTAB)
		{
			symtab_command sym_cmd;
			read(sym_cmd);

			const char* sym_begin = begin + sym_cmd.symoff;
			const char* str_tbl = begin + sym_cmd.stroff;

			for (int j = 0; j < sym_cmd.nsyms; ++j)
			{
				nlist_64 sym;
				read_from(sym, sym_begin);

				Assert(sym.n_strx < sym_cmd.strsize, "invalid string offset");

				auto ptr = sym.n_value;
				if (sym.n_sect > 0)
				{
					vas.push_back({ptr, convert_symbol_name(str_tbl + sym.n_strx)});
				}
				else
				{
				}
			}
		}
		else if (cmd.cmd == LC_UUID)
		{
			uuid_command uuid;
			read(uuid);

		}
		else
		{
			skip(cmd.cmdsize - sizeof(load_command));
		}
	}

	auto search_begin = vas.begin();
	auto search_end = vas.end();

	for (int i = 0; i < vas.size(); ++i)
	{
		vas[i].offset = vas[i].offset - base_addr;
	}

	std::stable_sort(vas.begin(), vas.end(), [](auto& a, auto& b){
		return a.offset < b.offset;
	});

	return vas;
}


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

#endif
