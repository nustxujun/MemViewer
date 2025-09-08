// MemoryTraceParser.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <regex>
#include <assert.h>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <format>
#include <future>
#include <chrono>
#include <set>
#include <map>
#include <string>
#include <list>
#include <ranges>
#include<sstream>

#include "MemoryTraceParser.h"
#include "SymbolParser.h"

#define LATEST_VERSION 7
#define VERSION LATEST_VERSION

#define VER_GRT_EQ(x) (VERSION >= x)

#define REMOVE_ONE_FRAME_ALLOC 1 // remove allocation infos which has only one frame lifecycle

struct TraceFiles
{
	std::string alloc_file_path;
    std::string dealloc_file_path;
	std::string callstack_file_path;
	std::string module_file_path;
    std::string snapshot_file_path;
    std::string symbol_file_path;
    std::string frame_file_path;
    std::string object_file_path;
    std::string rhi_file_path;
    std::string lua_file_path;
    std::string custom_file_path;
};

struct ObjectInfo
{
    uint64_t ptr;
    std::string name;
    //std::vector<std::pair<std::string, std::string>> infos;
    
    uint32_t classid;
    int stackid;
    uint32_t begin;
    uint32_t end = -1;

    uint32_t size = 0;
};

struct ObjectClassInfo
{
    std::vector<std::string> chain;
};

struct AllocInfo
{
#if VER_GRT_EQ(2)
    uint64_t ptr;
#endif
	uint32_t size;
	uint32_t trace;
	uint32_t start;
	uint32_t end;
    
};

struct Checkpoint
{
    std::string text;
    int FrameId;
};

struct DeallocInfo
{
    uint64_t ptr;
    uint32_t end;
    uint32_t padding;
};


struct FrameInfo
{
    uint32_t frame;
    uint64_t used;
    uint64_t available;
    int64_t overhead;
    
    std::map<std::string, uint32_t> customs;
};

struct RhiInfo
{
    enum{
        Texture,
        TextureArray,
        Texture3D,
        TextureCube,
        TextureCubeArray,
        
        Vertex,
        Index,
        Struct,
        Uniform,
    
    };
    uint64_t ptr;
    std::string name;
    uint32_t begin;
    uint32_t end = -1;
    uint32_t node_index = 0;
    uint32_t type;
    uint32_t size;
    uint32_t length[3];
    uint32_t format;
    uint32_t mips;
    uint32_t rt;
};

struct CustomData
{
    uint64_t ptr = 0;
    std::string name;
    uint32_t size = 0;
    uint32_t type = 0;
    uint32_t begin = 0;
    uint32_t end = -1;
    std::vector<char> data;
};
static auto split_string = [](const std::string& in, const std::string& delim)
{
    std::vector<std::string> ret;
    for (const auto& word : std::views::split(std::string(in), delim)) {
        ret.push_back(std::string(word.begin(), word.end()));
    }
    return ret;
};
static std::string convert_size(uint64_t size)
{
    const uint32_t k = 1024;
    const uint32_t m = k * k;

    if (size > m)
        return std::format("{0:.2f} MB", float(size) / m);
    else if (size > k)
        return std::format("{0:.2f} KB", float(size) / k);
    else
        return std::format("{0} Bytes", size);

}

struct Node
{
	using Ptr = std::shared_ptr<Node>;
	Node* parent = 0;
	uint64_t addr = 0;
	uint64_t size = 0;
	uint32_t count = 0;
    uint32_t index = 0;
	float rate = 100.0f;
	std::vector<Ptr> children;
	std::string name;

	
	Node()
    {
    }
	Node(Node* p, const std::string& n) : parent(p), name(n)
	{
	}

	Ptr findChild(const std::string& name)
	{
		for (Ptr n : children)
		{
			if (n->name == name)
				return n;
		}
		return {};
	}

	Ptr findOrCreateChild(const std::string& name)
	{
		auto n = findChild(name);
		if (n)
			return n;


		children.push_back(std::make_shared<Node>(this, name));
		return *children.rbegin();
	}

	void add(uint32_t val)
	{

		size += val;
		count++;
		if (parent)
		{
			parent->add(val);
		}
	}

	void print(std::fstream& f, int depth = 0)
	{
		f << std::format("{0:16}\t{1:.1f}%\t{2:16d}\t", convert_size(size), rate, count);
		for (int i = 0; i < depth; ++i)
			f << ' ';
		f << name << "\n";

		for (auto n : children)
		{
			n->print(f, depth + 1);
		}
	}

	void calRate(uint64_t total)
	{
		for (auto& n : children)
		{
			Assert(n->size <= size, "child not is greater than parent.");
			n->rate = (n->size * 100.0f) / total;
			n->calRate(total);
		}
	}

	void visit(const std::function<void(Node*, int)>& cb, int depth = 0)
	{
		cb(this, depth);
		for (auto& n : children)
		{
			n->visit(cb, depth + 1);
		}
	}
    
    void cull()
    {
        for(auto i = children.begin(); i < children.end();)
        {
            if ((*i)->count == 0)
                i = children.erase(i);
            else
            {
                (*i)->cull();
                ++i;
            }
        }
        
        std::sort(children.begin(), children.end(), [](auto a, auto b){
            return a->size > b->size;
        });
    }
    
    Node::Ptr clone()const
    {
        Node::Ptr new_node = std::make_shared<Node>(nullptr, name);
        new_node->name = name;
        new_node->size = size;
        new_node->count = count;
        new_node->rate = 100.0f;
        for(auto& child : children)
        {
            auto new_child = child->clone();
            new_child->parent = new_node.get();
            new_node->children.push_back(new_child);
        }
        
        return new_node;
    }
};

std::vector<std::filesystem::path> directories;
std::vector<std::filesystem::path> files;


static std::string findSingleFile(const std::string& file_name)
{
	std::regex pattern(file_name);

	std::smatch result;
    
    auto path = [&]()
    {
        for (auto& file : files)
        {
            if (std::regex_match(file.string(), pattern))
            {
                return file;
            }
        }
        
        
        for (auto& dir : directories)
        {
            for (const auto& file : std::filesystem::directory_iterator(dir))
            {
                if (!std::filesystem::is_directory(file) && std::regex_match((file.path().string()), pattern))
                {
                    return file.path();
                }
            }
            
            auto mix_path = dir.string() + file_name;
            if (std::filesystem::exists(mix_path) )
            {
                return std::filesystem::path(mix_path);
            }
        }
        
        return std::filesystem::path();
    }();
    if (path.empty())
        return {};
    path = std::filesystem::absolute(path);
    
    if (std::filesystem::exists(path))
        return path.string();
    else
        return {};
}

static std::vector<std::string> findFiles(const std::string& file_name)
{
	std::vector<std::string> results;
	std::regex pattern(file_name);
	for (auto& dir : directories)
	{
		for (const auto& file : std::filesystem::directory_iterator(dir))
		{
			if (std::regex_match(file.path().string(), pattern))
			{
				results.push_back(file.path().string());
			}
		}
	}

	for (auto& file : files)
	{
		if (std::regex_match(file.string(), pattern))
		{
			results.push_back(file.string());
		}
	}

    std::vector<std::string> paths;
    for (auto& path : results)
    {
        path = std::filesystem::absolute(path).string();
        if (std::filesystem::exists(path))
            paths.push_back(path);
    }
    
	return paths;
}

static TraceFiles findTraceFile()
{
	TraceFiles result;

	result.alloc_file_path = findSingleFile(".+[\\\\/]allocations");
	result.callstack_file_path = findSingleFile(".+stacks");
	result.module_file_path = findSingleFile(".+[\\\\/]modules");
    result.snapshot_file_path = findSingleFile(".+snapshots");
#if WIN64_PARSER
	result.symbol_file_path = findSingleFile(".+\\.pdb");

#elif IOS_PARSER
    if (result.symbol_file_path.empty())
    {
        result.symbol_file_path = findSingleFile(".+symbols");
        if (result.symbol_file_path.empty())
            result.symbol_file_path = findSingleFile("/Contents/Resources/DWARF/ProjectZClient");///Contents/Resources/DWARF/.+
    }
#endif
    
    result.object_file_path = findSingleFile(".+[\\\\/]objects");
    
#if VER_GRT_EQ(2)
    result.dealloc_file_path = findSingleFile(".+deallocations");
    result.frame_file_path = findSingleFile(".+frames");
#endif
    
    result.rhi_file_path = findSingleFile(".+[\\\\/]rhis");
    
    result.lua_file_path = findSingleFile(".+[\\\\/]luainfos");

    Log("alloc file: \n{0}\ncallstack file: \n{1}\nmodule file: \n{2}\nobject file: \n{3}", result.alloc_file_path, result.callstack_file_path, result.module_file_path,result.object_file_path);

    Log("symbol file:\n{0}\n", result.symbol_file_path);

    result.custom_file_path = findSingleFile(".+[\\\\/]customs");
    
	return result;
}




struct AsyncTask
{
    std::future<void> future;
    AsyncTask(std::future<void>&& f):future(std::move(f)){};
    AsyncTask(AsyncTask&& Task):future(std::move(Task.future)){}
    void Sync(){if (future.valid()) future.get();}
    ~AsyncTask(){Sync();}
};

AsyncTask async_run(std::function<void()>&& func)
{
    return AsyncTask(std::async(std::move(func)));
}

static bool skip_compression = false;

static void parse(const TraceFiles& file_names, const std::string& output_dir)
{
    Timer timer;
    timer.begin("\n\nload from files");
	auto read_buffer = [](auto& f, auto& val) {
		f.read((char*)&val, sizeof(val));
		return f.good();
	};
    
    auto read_string = [&](auto& f){
        uint32_t len = 0;
        read_buffer(f, len);
        std::string str;
        if (len == 0)
            return str;
        str.resize(len, 0);
        f.read(str.data(),len);
        return str;
    };
    
    auto skip_buffer = [](auto& f, size_t size){
        f.seekg(size, std::ios::beg);
    };
	std::fstream alloc_file = std::fstream(file_names.alloc_file_path, std::ios::in | std::ios::binary);
	std::fstream stack_file = std::fstream(file_names.callstack_file_path,std::ios::in | std::ios::binary);
	std::fstream module_file = std::fstream(file_names.module_file_path, std::ios::in | std::ios::binary);
    std::fstream snapshot_file = std::fstream(file_names.snapshot_file_path, std::ios::in | std::ios::binary);
    
#if VER_GRT_EQ(2)
    std::fstream dealloc_file = std::fstream(file_names.dealloc_file_path, std::ios::in | std::ios::binary);
    std::fstream frame_file = std::fstream(file_names.frame_file_path, std::ios::in | std::ios::binary);
    std::fstream object_file = std::fstream(file_names.object_file_path, std::ios::in | std::ios::binary);
    std::fstream rhi_file = std::fstream(file_names.rhi_file_path, std::ios::in | std::ios::binary);
#endif

#if VER_GRT_EQ(4)
    std::fstream lua_file=std::fstream(file_names.lua_file_path, std::ios::in | std::ios::binary);
#endif
#if VER_GRT_EQ(5)
    std::fstream custom_file = std::fstream(file_names.custom_file_path, std::ios::in | std::ios::binary);
#endif
#if VER_GRT_EQ(2)
    std::map<uint64_t,std::list<uint32_t>> dealloc_infos;
    std::vector<ObjectInfo> object_infos;
    std::vector<RhiInfo> rhi_infos;
    std::vector<ObjectClassInfo> obj_class_infos;
    auto dealloc_fut = async_run([&](){
        while (!dealloc_file.eof() && dealloc_file.good())
        {
            DeallocInfo info;
            dealloc_file.read((char*)& info, sizeof(info));

            if (dealloc_file.bad())
                break ;
            if (info.ptr == 0)
                continue;
            
//            dealloc_file.push_back(info);
            dealloc_infos[info.ptr].push_back(info.end);
            

        }
        
        return ;
    });
    

    
    auto object_fut = async_run([&](){
        std::map<uint64_t, uint32_t> obj_map;

        while (!object_file.eof() && object_file.good())
        {
            int state = 0;
            read_buffer(object_file, state);
            if (!object_file.good() || object_file.bad())
                return;
            if (state == 0)
            {
                int num = 0;
                read_buffer(object_file, num);
                ObjectClassInfo info;
                info.chain.reserve(num);
                for (int i = 0; i < num; ++i)
                {
                    info.chain.push_back(read_string(object_file));
                }

                if (!object_file.good() || object_file.bad())
                    return;

                obj_class_infos.push_back(std::move(info));
            }
            else if (state == 1)
            {
                uint64_t ptr;
                uint32_t time;

                read_buffer(object_file, ptr);
                read_buffer(object_file, time);

                auto ret = obj_map.find(ptr);
                if (ret == obj_map.end())
                {
                    Log("invalid obj delete event");
                }
                else
                {
                    auto& obj = object_infos[ret->second];
                    if (obj.begin > time)
                    {
                        Log("invalid obj end time, {:#x} {} {}, invalid end : {}", obj.ptr, obj.name, obj.begin, time);
                        return;
                    }
                    object_infos[ret->second].end = time;
                }
            }
            else if (state == 2)
            {
                ObjectInfo info;
                read_buffer(object_file, info.ptr);
                uint32_t time = 0;
                read_buffer(object_file, time);
                info.begin = time;

                read_buffer(object_file, info.classid);
                read_buffer(object_file, info.stackid);

                info.name = read_string(object_file);


                if (!object_file.good() || object_file.bad())
                    return;

                obj_map[info.ptr] = object_infos.size();
                object_infos.push_back(info);
            }
            else
            {
                Log("invalid object file data");
                return;
            }

            
        }
        return ;
    });
    
    std::map<uint64_t, std::vector<uint32_t>> rhi_refs;
    auto rhi_fut = async_run([&](){
        std::map<uint64_t, uint32_t> rhi_map;
        std::vector<uint32_t> ordered_rhi;
        while(!rhi_file.eof() && rhi_file.good())
        {
            uint32_t is_create;
            read_buffer(rhi_file, is_create);
            if (!rhi_file.good())
                break;
            RhiInfo info;
            read_buffer(rhi_file, info.ptr);
            
            if (is_create)
            {
                uint32_t time;
                read_buffer(rhi_file, time);
                auto ret = rhi_map.find(info.ptr);
                if (ret != rhi_map.end())
                {
                    if (rhi_infos[ret->second].end == -1)
                    {
                        rhi_infos[ret->second].end = time;
//                        Log("lost delete event on {:#x} {}", rhi_infos[ret->second].ptr, rhi_infos[ret->second].name);
                    }
                }
                rhi_map[info.ptr] = rhi_infos.size();
                info.begin = time;

            }
            else
            {
//                Assert(rhi_map.find(info.ptr) != rhi_map.end(), "invalid rhi delete event");
                if (rhi_map.find(info.ptr) == rhi_map.end())
                {
                    static int errcount = 0;
                    errcount++;
                    uint32_t time;
                    read_buffer(rhi_file, time);
                    if (errcount < 100)
                        Log("invalid rhi delete event {:#x} {}", info.ptr, time);
                    continue;
                }
                auto& rhi = rhi_infos[rhi_map[info.ptr]];
                
                uint32_t time = 0;
                read_buffer(rhi_file, time);
                if (rhi.end == -1 || rhi.end > time)
                {
                    rhi.end = time;
                }

                if (rhi.begin > rhi.end)
                {
                    Log("invalid rhi time event (begin > end), ptr: {:#x} begin: {} , end : {}", rhi.ptr, rhi.begin, rhi.end );

                    Log("address {:#x} history:\n", rhi.ptr);
                    int idx = 0;
                    for (auto& r : rhi_infos)
                    {
                        if (r.ptr != rhi.ptr)
                            continue;


                        Log("begin: {} end: {} type: {}", r.begin, r.end,r.type);
                    }


                    Log("total rhis: {}", rhi_infos.size());
                    rhi_infos.clear();
                    return ;
                }
                //Assert(rhi.begin <= rhi.end, "invalid rhi time event");
                continue;
            }
            
            info.name = read_string(rhi_file);
            
            read_buffer(rhi_file, info.type);
            read_buffer(rhi_file, info.size);
            
            auto check_pooled_buffer = [&](){
                
                for(auto i = ordered_rhi.begin(); i != ordered_rhi.end(); )
                {
                    auto& rhi =rhi_infos[*i];
                    if (rhi.end <= info.begin)
                    {
                        i = ordered_rhi.erase(i);
//                        Log("{:#x} {} {} {} {}", rhi.ptr, rhi.name, rhi.begin, rhi.end, info.begin);
                        continue;
                    }
                    
                    if ( rhi.ptr + rhi.size <= info.ptr)
                    {
                        ++i;
                        continue;
                    }
                    if (rhi.ptr >= info.ptr + info.size )
                    {
                        ordered_rhi.insert(i,rhi_infos.size() );
                        return;
                    }
//                    Log("{:#x} {} is end {} {}", rhi.ptr, rhi.name, rhi.end, info.begin);
                    rhi.end = std::min(rhi.end, info.begin);
                    i = ordered_rhi.erase(i);
                }
                
                ordered_rhi.push_back(rhi_infos.size());
            };
            
            
            
            switch(info.type)
            {
                case RhiInfo::Vertex:
                case RhiInfo::Index:
                case RhiInfo::Struct:
                case RhiInfo::Uniform:
                    check_pooled_buffer();
                    break;
                case RhiInfo::Texture:
                case RhiInfo::TextureArray:
                case RhiInfo::Texture3D:
                case RhiInfo::TextureCube:
                case RhiInfo::TextureCubeArray:
                    read_buffer(rhi_file, info.length[0]);
                    read_buffer(rhi_file, info.length[1]);
                    read_buffer(rhi_file, info.length[2]);
                    
                    read_buffer(rhi_file, info.format);
                    read_buffer(rhi_file,info.mips);
                    read_buffer(rhi_file,info.rt);
                    break;
                default:
                    Log("invalid rhi type at {}", rhi_infos.size());
                    Log("last rhi ptr:{:#x} begin: {} end: {}", rhi_infos.rbegin()->ptr, rhi_infos.rbegin()->begin, rhi_infos.rbegin()->end);
                    rhi_infos.clear();
                    return ;
            }

            if (!rhi_file.good() || rhi_file.bad())
                return;

            rhi_refs[info.ptr].push_back(rhi_infos.size() );
            rhi_infos.push_back(info);
        }

    });
#endif
    
#if VER_GRT_EQ(4)
    std::vector<AllocInfo> lua_alloc_infos;
    Node::Ptr lua_root = std::make_shared<Node>();
    Node* cur_node = lua_root.get();
    uint32_t num_lua_node = 2;
    auto lua_alloc_fut = async_run ([&](){
        auto unknown = lua_root->findOrCreateChild("unknown");
        unknown->index = 1;
        std::map<uint64_t, uint32_t> mapped_alloc;
        while(!lua_file.eof() && lua_file.good())
        {
            int type;
            read_buffer(lua_file, type);
            if (lua_file.bad() || !lua_file.good())
                return;
            switch(type)
            {
                case 0:
                {
                    std::string name = read_string(lua_file);
                    cur_node = cur_node->findOrCreateChild(name).get();
                    if (cur_node->index == 0)
                    {
                        cur_node->index = num_lua_node++;
                    }
                }
                    break;
                case 1:
                    cur_node = cur_node->parent;
                    break;
                case 2:
                {
                    AllocInfo alloc;
                    read_buffer(lua_file, alloc.ptr);
                    read_buffer(lua_file,alloc.size);
                    read_buffer(lua_file, alloc.start);
                    alloc.end = -1;
                    if (cur_node == lua_root.get())
                        alloc.trace = unknown->index;
                    else
                        alloc.trace = cur_node->index;
                    lua_alloc_infos.push_back(alloc);
                    mapped_alloc[alloc.ptr] =lua_alloc_infos.size() - 1;
                }
                    break;
                case 3:
                {
                    uint64_t ptr;
                    uint32_t end;
                    read_buffer(lua_file,ptr);
                    read_buffer(lua_file, end);
                    
                    auto ret = mapped_alloc.find(ptr);
                    if (ret != mapped_alloc.end())
                    {
                        auto alloc =&lua_alloc_infos[ret->second];
                        if (alloc->end != -1)
                        {
                            
                            Assert(alloc->end == -1, "invalid lua free");
                        }
                        alloc->end = end;
                    }
                }
                    break;
                default:
                    Error("failed to read lua infos");
                    return;
            }
        };
    });
#endif

#if VER_GRT_EQ(5)
    std::vector< CustomData> custom_infos;
    auto custom_fut = async_run([&](){
        std::unordered_map<uint64_t, int> custom_map;
        while (!custom_file.eof() && custom_file.good())
        {
            int is_create = false;
            read_buffer(custom_file,is_create);
            uint64_t ptr;
            read_buffer(custom_file, ptr);
            if (is_create)
            {
                CustomData data;
                data.ptr = ptr;
                read_buffer(custom_file, data.begin);
                data.name = read_string(custom_file);
                read_buffer(custom_file, data.size);
                read_buffer(custom_file, data.type);
                uint32_t size;
                read_buffer(custom_file, size);

                if (size > 0)
                {
                    data.data.resize(size);
                    custom_file.read(data.data.data(), size);
                }
                custom_map[data.ptr] = custom_infos.size();
                custom_infos.push_back(std::move(data));
            }
            else
            {
                uint32_t end;
                read_buffer(custom_file, end);

                auto ret = custom_map.find(ptr);
                if (ret != custom_map.end())
                {
                    auto& data = custom_infos[ret->second];
                    if (data.begin <= end)
                    {
                        data.end = end;
                    }
                    custom_map.erase(ret);
                }
            }

        }
    });
#endif

	std::vector<AllocInfo> alloc_infos;
	auto alloc_fut = async_run([&](){
        while (!alloc_file.eof() && alloc_file.good())
        {
            AllocInfo info;
            alloc_file.read((char*)& info, sizeof(info));

            if (alloc_file.bad())
                return ;
            if (info.size == 0)
                continue;

            alloc_infos.push_back(info);
        }
        return ;
    });

	std::unordered_set<uint64_t> unique_addrs;
	std::vector<std::vector<uint64_t>> stack_map;

	auto stack_fut = async_run([&](){

        while (!stack_file.eof() && stack_file.good())
        {
            uint64_t count;
            stack_file.read((char*)&count, sizeof(count));
            if (count == 0)// end of file
            {
                continue;
//                return true;
            }
            if (alloc_file.bad())
                return false;

            std::vector<uint64_t> addrs;
            addrs.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                uint64_t addr;
                stack_file.read((char*)&addr, sizeof(addr));
                if (addr == 0) // end of file
                    return true;
                unique_addrs.insert(addr);
                addrs.push_back(addr);
            }

            if (alloc_file.bad())
                return false;
            
            std::reverse(addrs.begin(), addrs.end());
            stack_map.push_back(std::move(addrs));
        }
        return true;
    });

	std::vector<ModuleInfo> modules;
	{

		uint32_t count ;
		if (read_buffer(module_file, count))
		{
			for (uint32_t i = 0; i < count; ++i)
			{
				ModuleInfo mi;
				read_buffer(module_file,mi.base_addr);
				read_buffer(module_file, mi.size);
				int len;
				read_buffer(module_file, len);
				mi.name.resize(len);
				module_file.read(mi.name.data(),len);

				
				int pos = -1;
				int cur = mi.name.size();
				for (auto i = mi.name.rbegin(); i != mi.name.rend(); ++i)
				{
					cur -= 1;
					if ((*i) == '\\' || (*i) == '/')
					{
						pos = cur;
						break;
					}
				}

				mi.name = mi.name.substr(pos + 1);
				cur = 0;
				for (auto i = mi.name.begin(); i != mi.name.end(); ++i)
				{
					if ((*i) == '.' )
					{
						mi.name = mi.name.substr(0, cur);
						break;
					}
					cur++;
				}

				modules.push_back(std::move(mi));
			}
		}



		std::sort(modules.begin(), modules.end(), [](auto& a, auto& b){
			return a.base_addr < b.base_addr;
		});

        size_t total_modules = 0;
        for (auto& m : modules)
        {
            total_modules += m.size;
        }


        Log("total module size: {}", total_modules);


	}

//	for (auto& path : file_names.symbol_file_paths)
	{
		OpenSymbolFile(file_names.symbol_file_path.c_str());
	}
    
    std::vector<Checkpoint> checkpoints;
    {
        while(!snapshot_file.eof() )
        {
            auto count = snapshot_file.tellg();
            Checkpoint cp;
            if (!read_buffer(snapshot_file, cp.FrameId))
                break;
            cp.text = read_string(snapshot_file);

            checkpoints.push_back(std::move(cp));
            
            
            
        }

    }
#if VER_GRT_EQ(2)
    std::vector<FrameInfo> frames;
    {
        while(!frame_file.eof() && frame_file.good())
        {
            FrameInfo info;
            read_buffer(frame_file, info.frame);
            read_buffer(frame_file,info.used);
            read_buffer(frame_file, info.available);
            read_buffer(frame_file, info.overhead);
#if VER_GRT_EQ(7)
            uint32_t len = 0;
            read_buffer(frame_file, len);
            if (len != 0)
            {
                int platform ;
                read_buffer(frame_file, platform);
                
                if (platform == 2)// ios
                {
                    int count;
                    read_buffer(frame_file, count);
                    
                    for (int i = 0; i < count; ++i)
                    {
                        std::string name = read_string(frame_file);
                        int size;
                        read_buffer(frame_file, size);
                        info.customs[name] = size;
                    }
                }
                else
                {
                    skip_buffer(frame_file,len - 4);
                }
            }
#endif
            frames.push_back(info);
        }
    }
#endif
    

#if VER_GRT_EQ(2)


    dealloc_fut.Sync();
#endif
    
    alloc_fut.Sync();
    stack_fut.Sync();
    
    
    timer.end();
    Log("get {0} allocations", alloc_infos.size());
    Log("get {0} stacks", stack_map.size());
    timer.begin("parse symbols");
    
//    Assert(alloc_infos.size() > 0, "can not get any allocation infos.");
    
	std::vector<uint64_t> addrs;
	std::vector<std::string> names;
	addrs.reserve(unique_addrs.size());
	for (auto addr : unique_addrs)
	{
		addrs.push_back(addr);
	}

    std::sort(addrs.begin(), addrs.end());
	ParseSymbolByVA(addrs,modules,names);

    struct InternalFunctionInfo
    {
        uint64_t start_addr;
        std::string name;
    };
    std::map<uint64_t, InternalFunctionInfo> function_map;
    auto addr_count = addrs.size();
    std::regex pattern("(.+) \\(in (.+)");
    for (size_t i = 0; i < addr_count; ++i)
    {
        
        auto addr = addrs[i];
        std::smatch ret;
        if (std::regex_match(names[i], ret, pattern))
        {
            function_map[addr] = {0, ret[1].str()};
        }
        else
        {
            function_map[addr] = {0, names[i]};
        }
    }
    timer.end();
    
    timer.begin("make call tree");
        
    Node::Ptr root = std::make_shared<Node>();
    root->name = "root";
    auto unknown = root->findOrCreateChild("unknown");
    unknown->index = 1;
    uint32_t num_node = 2;
    std::vector< Node::Ptr> node_map;
    node_map.reserve(stack_map.size());
    for (auto& stack : stack_map)
    {
        auto cur = root;
        for (auto addr : stack)
        {
            auto func_info = function_map.find(addr);
            Assert(func_info != function_map.end(), "function can not be lost" );
            auto n = cur->findOrCreateChild(func_info->second.name);
            if (n->index == 0)
            {
                n->index = num_node++;
            }
            cur = n;
        }
        if (cur == root)
        {
            Log("callstack {0} has no backtrace infos", node_map.size());
            cur = unknown;
        }
        node_map.push_back(cur);
 
    }


    timer.then("output file");
    std::fstream output_file(output_dir, std::ios::out | std::ios::binary);
    auto write = [&](const auto& val){
        output_file.write((const char*)&val, sizeof(val));
    };
    
    auto write_string = [&](const std::string& val){
        int len = val.length();
        write(len);
        if (len > 0)
            output_file.write(val.c_str(), len);
    };
    
    int version = VERSION;
    write(version);
    
    uint32_t cur_index = 0;
    Log("record {0} stack nodes", num_node);
    write(num_node);
    root->visit([&](auto node, int depth){
        node->index = cur_index++;
        write(depth);
//        write(cur_index++);
//        write(stackid);
        write_string(node->name);
//        write(node->size);
//        write(node->count);
//        write(node->rate);
    });
    
    Assert(cur_index == num_node," node count is invalid.");
    
    
    const int end_tag = -1;
    write(end_tag);
    
    // write((uint32_t)alloc_infos.size());
    
    
    
#if VER_GRT_EQ(3)
        rhi_fut.Sync();
    Log("Get {} rhi infos", rhi_infos.size());
#endif
    
    uint32_t num_frames = 0;
    uint32_t duplicated_ptr = 0;
    uint32_t num_allocs = 0;
    for (auto& info : alloc_infos)
    {
 
        uint32_t node_index = 0;
        if (info.trace < node_map.size() )
            node_index = node_map[info.trace]->index;
        else
        {
            static int count = 0;
            if (count++ < 10)
                Log("failed to find stack by trace id {0}", info.trace);
        }
#if VER_GRT_EQ(2)
        {
            //        dealloc_infos.erase(info.ptr);
            auto ret = dealloc_infos.find(info.ptr);
            if (ret != dealloc_infos.end() && ret->second.size() > 0)
            {
                uint32_t end_frame  = 0 ;
                do
                {
                    end_frame = *ret->second.begin();
                    ret->second.pop_front();
                }while (end_frame < info.start && ret->second.size() > 0);
                
                if (end_frame >= info.start)
                    info.end = end_frame;
                
            }
            else{
                info.end = 0;
            }
            
        }
#endif
#if REMOVE_ONE_FRAME_ALLOC
        if (info.start != info.end)
#endif
            num_allocs++;
        
//        write(info.size);
//        write(node_index);
//        write(info.start);
//        write(info.end);
        info.trace = node_index;

        
        num_frames = std::max(num_frames, info.start);
        num_frames = std::max(num_frames, info.end);
    }
    
    
    write(num_allocs);
    
    for (auto& info : alloc_infos)
    {
#if REMOVE_ONE_FRAME_ALLOC
        if (info.start != info.end)
#endif
        {
            write(info.size);
            write(info.trace);
            write(info.start);
            write(info.end);
        }
    }

    Log("the max frame in allocations is {0}", num_frames);
#if VER_GRT_EQ(2)
    Log("the discard realloc info {0}", dealloc_infos.size());
#endif
    

    
    const int num_snapshots = checkpoints.size();
    write(num_snapshots);
    for(auto& cp : checkpoints)
    {
        write(cp.FrameId);
        write_string(cp.text);
    }
    
#if VER_GRT_EQ(2)
    num_frames = frames.size();
    Log("Get {0} frames", num_frames);
    write(num_frames);
    for (auto& info : frames)
    {
        write(info.used);
        write(info.available);
        write(info.overhead);
#if VER_GRT_EQ(7)
        int count = info.customs.size();
        write(count);
        for (auto& i : info.customs)
        {
            write_string(i.first);
            write(i.second);
        }
#endif
    }
    
    object_fut.Sync();
    Log("get {0} objects\n", object_infos.size());
    
#if VER_GRT_EQ(6)  
    uint32_t num_obj_class = obj_class_infos.size();
    write(num_obj_class);

    for (auto& info : obj_class_infos)
    {
        uint32_t count = info.chain.size();
        write(count);
        for (auto& n : info.chain)
        {
            write_string(n);
        }
    }
#endif
    
    uint32_t num_objs = object_infos.size();
    write(num_objs);
    
    for (auto& info: object_infos)
    {
        write_string(info.name);
        write(info.classid);
        uint32_t node_index = 0;
        if (info.stackid >= 0 && info.stackid < node_map.size())
            node_index = node_map[info.stackid]->index;
        else
        {
            static int count = 0;
            if (count++ < 10)
                Log("failed to find stack by stack id {0}", info.stackid);
        }

        write(node_index);
        write(info.begin);


        auto end = info.end;
        if (end == -1)
            end = num_frames - 1;
        write(end);
        
    }
    
#endif

#if VER_GRT_EQ(3)

    {
        rhi_fut.Sync();
        Log("Get {} rhi infos", rhi_infos.size());
        int count = rhi_infos.size();
        write(count);
        [&]()
        {
            for (int i = 0; i < count; ++i)
            {
                auto& info = rhi_infos[i];
            
                write(info.ptr);
                write_string(info.name);
                write(info.begin);
            
                if (info.end == 0 || info.end == -1)
                {
                    info.end = num_frames - 1;
                }

                write(info.end);
                write(info.node_index);
                write(info.type);
                write(info.size);
                switch(info.type)
                {
                    case RhiInfo::Vertex:
                    case RhiInfo::Index:
                    case RhiInfo::Struct:
                    case RhiInfo::Uniform:
                        break;
                    case RhiInfo::Texture:
                    case RhiInfo::TextureArray:
                    case RhiInfo::Texture3D:
                    case RhiInfo::TextureCube:
                    case RhiInfo::TextureCubeArray:
                        write(info.length[0]);
                        write(info.length[1]);
                        write(info.length[2]);
                    
                        write( info.format);
                        write(info.mips);
                        write(info.rt);
                        break;
                    default:
                        Error("invalid rhi type , when write to file");

                        return;
                    
                }
            }

        }();
        
    }
#endif
    
#if VER_GRT_EQ(4)
    lua_alloc_fut.Sync();
    
    write(num_lua_node);
    lua_root->visit([&](auto node, int depth){
        write(depth);
        write_string(node->name);
    });
    write(end_tag);
    
    uint32_t lua_count = lua_alloc_infos.size();
    write(lua_count);
    for (auto& alloc : lua_alloc_infos)
    {
        if (alloc.end == -1)
            alloc.end = 0;
        write(alloc.size);
        write(alloc.trace);
        write(alloc.start);
        write(alloc.end);
    }
#endif
    
    
#if VER_GRT_EQ(5)
    custom_fut.Sync();

    int num_customs = custom_infos.size();
    write(num_customs);

    for (auto& data : custom_infos)
    {
        if (data.end == -1)
        {
            data.end = 0;
        }

        write(data.type);
        write_string(data.name);
        write(data.size);
        write(data.begin);
        write(data.end);
        write((uint32_t)data.data.size());
        if (data.data.size()> 0)
            output_file.write(data.data.data(), data.data.size());
    }
#endif

    
    
    
    
    // stop serializing
    output_file.close();

    if (!skip_compression)
    {
        timer.then("compress");
        auto zip_path = output_dir + ".zip";
        std::filesystem::remove(zip_path);
    
#ifdef PLATFORM_MAC
        system(std::format("zip -j {0} {1}", zip_path, output_dir).c_str());
#else
        auto dir = std::filesystem::path(output_dir).parent_path().string();
        auto name = std::filesystem::path(output_dir).filename().string();
        auto cmd = std::format("tar -czvf {} -C {} {}", zip_path, dir, name);
        system(cmd.c_str());
#endif
    }


}



int main(int num, char** args)
{
    Log("\ncurrent version is {}\n", VERSION);
    
    std::string output_dir;
	for (int i = 1; i < num; ++i)
	{
		std::string path = args[i];
        if (path[0] == '-' && path[1] == 'o' && path[2] == '=')
        {
            path = path.substr(3);
//            Assert(std::filesystem::is_file(path), "output is not a file");
            
            output_dir = path;
        }
        else if (path[0] == '-' && path[1] == 'q')
        {
            skip_compression = true;
        }
        else
        {
            if (std::filesystem::is_directory(path))
                directories.push_back(path);
            else
                files.push_back(path);
        }
	}

    if (directories.empty())
        directories.push_back("./");

    if (output_dir.empty())
    {
        skip_compression = true;
        output_dir = "trace.memtrace";
    }

	auto trace_files = findTraceFile();
	parse(trace_files, output_dir);



}
