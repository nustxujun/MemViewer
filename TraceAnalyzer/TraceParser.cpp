#include "CoreTypes.h"
#include <fstream>
#include <unordered_map>
#include <cassert>
#include <filesystem>
#include <format>

bool TraceData::ParseTraceFile(const std::string& path, TraceData* trace)
{
	Counter counter("load file");

	std::filesystem::path file_path = path;
	auto ext = file_path.extension();
	std::string trace_path = path;

	if (ext == ".zip")
	{
		auto caches_dir = std::filesystem::current_path() / "caches";
		std::filesystem::create_directories(caches_dir);
		auto cmd = std::format("tar -xf \"{}\" -C \"{}\"", file_path.string(), caches_dir.string());
		std::system(cmd.c_str());

		trace_path.clear();
		for (auto& entry : std::filesystem::directory_iterator(caches_dir))
		{
			if (entry.path().extension() == ".memtrace")
			{
				trace_path = entry.path().string();
				break;
			}
		}
		if (trace_path.empty())
		{
			Error("No .memtrace file found in ZIP archive");
			return false;
		}
		Log("Extracted: {}", trace_path);
	}

	auto trace_file = std::fstream(trace_path, std::ios::in | std::ios::binary);
	if (!trace_file)
		return false;

	auto file_size = std::filesystem::file_size(trace_path);
	std::string file_content(file_size, 0);
	trace_file.read(file_content.data(), file_size);

	char* cur_pos = file_content.data();
	char* end_pos = file_content.data() + file_size;

	auto read = [&](auto& val) -> bool {
		if (cur_pos + sizeof(val) > end_pos)
		{
			Warning("read out of range at offset {}", cur_pos - file_content.data());
			return false;
		}
		memcpy(&val, cur_pos, sizeof(val));
		cur_pos += sizeof(val);
		return true;
	};

	auto read_string = [&]() -> std::string {
		uint32_t len;
		if (!read(len))
			return {};
		if (len > 100 * 1024 * 1024 || cur_pos + len > end_pos)
		{
			Warning("read_string: invalid length {} at offset {}", len, cur_pos - file_content.data());
			return {};
		}
		std::string str;
		if (len > 0)
		{
			str.resize(len);
			memcpy(&str[0], cur_pos, len);
			cur_pos += len;
		}
		return str;
	};

	auto read_data = [&](char* data, size_t size) -> bool {
		if (cur_pos + size > end_pos)
		{
			Warning("read_data out of range at offset {}", cur_pos - file_content.data());
			return false;
		}
		memcpy(data, cur_pos, size);
		cur_pos += size;
		return true;
	};

	auto skip = [&](size_t size) -> bool {
		if (cur_pos + size > end_pos)
		{
			Warning("skip out of range at offset {}", cur_pos - file_content.data());
			return false;
		}
		cur_pos += size;
		return true;
	};

	uint32_t version = 0;
	if (!read(version))
		return false;

	uint32_t stack_len = 0;
	if (!read(stack_len))
		return false;

	std::vector<int> node_map;
	node_map.reserve(stack_len);

	Calltree::Ptr calltree = Calltree::create(stack_len);
	uint32_t cur_depth = 0;

	if (!read(cur_depth))
		return false;
	calltree->root->name = read_string();
	node_map.push_back(0);

	auto parent = calltree->root;
	Node* cur = parent;
	int node_count = 1;

	for (int i = 1;; ++i)
	{
		uint32_t depth;
		if (!read(depth) || depth == (uint32_t)-1)
		{
			assert(i == (int)stack_len);
			break;
		}

		if (depth - 1 == cur_depth)
		{
		}
		else if (depth <= cur_depth)
		{
			do
			{
				parent = *parent->parent;
				cur_depth--;
			} while (depth - 1 != cur_depth);
		}
		else
		{
			cur_depth++;
			parent = cur;
			if (cur_depth + 1 != depth)
			{
				assert(0);
				break;
			}
		}

		cur = new ((*calltree)[node_count]) Node();
		node_map.push_back(node_count);
		cur->name = read_string();
		cur->parent = {calltree, calltree->getIndex(parent)};
		parent->children.emplace_back(calltree, node_count);
		node_count++;
	}

	Assert(node_count == (int)stack_len, "invalid node count, sum {} record {}", node_count, stack_len);

	std::vector<AllocInfo> allocs;
	uint32_t alloc_count;
	if (!read(alloc_count))
		return false;
	allocs.reserve(alloc_count);

	uint32_t start = (uint32_t)-1;
	uint32_t end = 1;
	std::vector<uint32_t> zero_end;
	zero_end.reserve(alloc_count);

	for (uint32_t i = 0; i < alloc_count; ++i)
	{
		AllocInfo ai;
		uint32_t index;

		if (!read(ai.size) || !read(index) || !read(ai.start) || !read(ai.end))
			return false;

		if (version >= 8)
		{
			if (!read(ai.user_tag))
				return false;
		}

		if (ai.end == 0)
			zero_end.push_back((uint32_t)allocs.size());

		start = std::min(start, ai.start);
		end = std::max(end, ai.end);
		end = std::max(end, ai.start);

		if (index < node_map.size())
			ai.node = {calltree, node_map[index]};
		else
		{
			Warning("alloc index {} out of range", index);
			continue;
		}

		if (ai.size < 1024 * 1024 * 1024)
			allocs.push_back(ai);
	}

	int num_snapshots = 0;
	if (!read(num_snapshots))
		return false;

	std::vector<Checkpoint> chkpnts;
	for (int i = 0; i < num_snapshots; ++i)
	{
		Checkpoint cp;
		if (!read(cp.frameid))
			return false;
		cp.text = read_string();
		chkpnts.push_back(cp);
	}

	std::vector<TotalInfo> totals;
	std::vector<ObjectInfo> objects;

	if (version >= 2)
	{
		uint32_t num_frame = 0;
		if (!read(num_frame))
			return false;

		totals.reserve(num_frame);
		for (uint32_t i = 0; i < num_frame; ++i)
		{
			TotalInfo info;
			if (!read(info.used) || !read(info.available))
				return false;
			if (version > 2)
			{
				if (!read(info.overhead))
					return false;
			}

			if (version >= 7)
			{
				int count;
				if (!read(count))
					return false;
				for (int j = 0; j < count; ++j)
				{
					auto key = read_string();
					uint32_t value;
					if (!read(value))
						return false;
					if (!key.empty())
						info.custom_datas.emplace(std::move(key), value);
				}
			}

			totals.push_back(info);
		}

		start = 1;
		end = num_frame;

		if (version >= 6)
		{
			uint32_t num_class = 0;
			if (!read(num_class))
				return false;

			std::map<int, ObjClassInfo::Ptr> obj_class_map;
			for (uint32_t i = 0; i < num_class; ++i)
			{
				auto info = std::make_shared<ObjClassInfo>();
				int count = 0;
				if (!read(count))
					return false;
				for (int j = 0; j < count; ++j)
				{
					info->name_chain.push_back(read_string());
				}
				std::reverse(info->name_chain.begin(), info->name_chain.end());
				obj_class_map.emplace(i, info);
			}

			uint32_t num_objects = 0;
			if (!read(num_objects))
				return false;

			for (uint32_t i = 0; i < num_objects; ++i)
			{
				ObjectInfo info;
				info.name = read_string();
				int class_id;
				if (!read(class_id))
					return false;
				info.class_chain = obj_class_map[class_id];

				int node_index;
				if (!read(node_index))
					return false;
				info.node = {calltree, node_index};
				if (!read(info.begin) || !read(info.end))
					return false;
				if (info.end == 0)
					info.end = end;
				objects.push_back(std::move(info));
			}
		}
		else
		{
			uint32_t num_objects = 0;
			if (!read(num_objects))
				return false;

			for (uint32_t i = 0; i < num_objects; ++i)
			{
				read_string();
				read_string();
				int num;
				read(num); read(num); read(num); read(num);
				int count;
				if (!read(count))
					return false;
				for (int j = 0; j < count; ++j)
				{
					read_string();
					read_string();
				}
			}
		}
	}

	for (auto idx : zero_end)
	{
		allocs[idx].end = end;
	}
	zero_end.clear();

	std::vector<RhiInfo> rhis;
	if (version >= 3)
	{
		uint32_t num_rhi = 0;
		if (!read(num_rhi))
			return false;
		rhis.reserve(num_rhi);
		for (uint32_t i = 0; i < num_rhi; ++i)
		{
			RhiInfo info;
			uint64_t ptr;
			if (!read(ptr))
				return false;
			info.ptr = ptr;
			info.name = read_string();
			if (!read(info.begin) || !read(info.end) || !read(info.node_index))
				return false;

			uint32_t attr;
			if (!read(info.type) || !read(info.size))
				return false;

			switch (info.type)
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
				for (int j = 0; j < 6; ++j)
				{
					if (!read(attr))
						return false;
					info.attrs.push_back(attr);
				}
				break;
			default:
				Warning("failed to parse rhi on index: {}", i);
				return false;
			}

			rhis.push_back(info);
		}
	}

	std::vector<AllocInfo> lua_allocs;
	Calltree::Ptr lua_calltree;
	uint32_t num_lua_node = 0;

	if (version >= 4)
	{
		node_map.clear();
		if (!read(num_lua_node))
			return false;

		lua_calltree = Calltree::create(num_lua_node);
		uint32_t lua_cur_depth = 0;

		if (!read(lua_cur_depth))
			return false;
		lua_calltree->root->name = read_string();
		node_map.push_back(0);

		auto lua_parent = lua_calltree->root;
		Node* lua_cur = lua_parent;
		int lua_node_count = 1;

		for (int i = 1;; ++i)
		{
			uint32_t depth;
			if (!read(depth) || depth == (uint32_t)-1)
			{
				assert(i == (int)num_lua_node);
				break;
			}

			assert(depth < 1024);

			if (depth - 1 == lua_cur_depth)
			{
			}
			else if (depth <= lua_cur_depth)
			{
				do
				{
					lua_parent = *lua_parent->parent;
					lua_cur_depth--;
				} while (depth - 1 != lua_cur_depth);
			}
			else
			{
				lua_cur_depth++;
				lua_parent = lua_cur;
				if (lua_cur_depth + 1 != depth)
				{
					assert(0);
					break;
				}
			}

			node_map.push_back(lua_node_count);
			lua_cur = new ((*lua_calltree)[lua_node_count]) Node();
			lua_cur->name = read_string();
			lua_cur->parent = {lua_calltree, lua_calltree->getIndex(lua_parent)};
			lua_parent->children.emplace_back(lua_calltree, lua_node_count);
			lua_node_count++;
		}

		uint32_t num_lua_alloc = 0;
		if (!read(num_lua_alloc))
			return false;

		for (uint32_t i = 0; i < num_lua_alloc; ++i)
		{
			AllocInfo ai;
			uint32_t index;

			if (!read(ai.size) || !read(index) || !read(ai.start) || !read(ai.end))
				return false;

			if (ai.end == 0)
				zero_end.push_back((uint32_t)lua_allocs.size());

			start = std::min(start, ai.start);
			end = std::max(end, ai.end);
			end = std::max(end, ai.start);

			Assert(index < num_lua_node, "invalid lua alloc index");
			ai.node = {lua_calltree, node_map[index]};
			lua_allocs.push_back(ai);
		}

		for (auto idx : zero_end)
		{
			lua_allocs[idx].end = end;
		}
	}

	std::vector<CustomData::Ptr> customs;
	if (version >= 5)
	{
		uint32_t num_objs = 0;
		if (!read(num_objs))
			return false;

		std::vector<char> buffer;
		for (uint32_t i = 0; i < num_objs; ++i)
		{
			int type;
			if (!read(type))
				return false;
			auto data = CustomData::createCustomData(type);
			data->type = type;
			data->name = read_string();
			if (!read(data->size) || !read(data->begin) || !read(data->end))
				return false;

			uint32_t size;
			if (!read(size))
				return false;
			if (size > 0)
			{
				buffer.resize(size);
				if (!read_data(buffer.data(), size))
					return false;
				data->serialize(buffer.data(), size);
			}
			customs.push_back(data);
		}
	}

	trace->lua_calltree = lua_calltree;
	trace->lua_allocs = std::move(lua_allocs);
	trace->calltree = calltree;
	trace->allocs = std::move(allocs);
	trace->memoryinfos = std::move(totals);
	trace->objects = std::move(objects);
	trace->rhis = std::move(rhis);
	trace->checkpoints = std::move(chkpnts);
	trace->max_range.begin = start;
	trace->max_range.end = end;
	trace->customs = std::move(customs);

	return true;
}
