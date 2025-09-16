#include "TraceParser.h"
#include "FrameParser.h"
#include "Utils.h"
#include "Concurrency.h"
#include <fstream>
#include <regex>
#include <unordered_map>
#include <assert.h>
#include <filesystem>
#include <format>
#include <mutex>
#include "TraceInstance.h"

#pragma optimize("",off)
CustomData::Ptr CustomData::createCustomData(int type)
{
	return std::make_shared<CustomData>();
}

bool TraceData::ParseTraceFile(const std::string& path, TraceData* trace)
{
	Counter counter("load file");

	std::filesystem::path file_path = path;
	auto ext = file_path.extension();
	std::string trace_path;
	if (ext == ".zip")
	{
		auto work_dir = std::filesystem::current_path();
		auto extra_dir = work_dir / "caches";
		std::filesystem::create_directories(extra_dir);

		std::regex pattern("([^/]+\\.memtrace)");
		auto filename = run_cmd(std::format("tar -tf {0}", file_path.string()));

		std::string trace_filename;
		for (auto& file : split_string(filename, "\n"))
		{
			std::smatch result;
			if (!std::regex_search(filename, result, pattern))
			{
				continue;
			}

			trace_filename = result[1];
			break;
		}

		if (trace_filename.empty())
			return false;

		trace_path = (extra_dir / trace_filename).string();
		std::filesystem::remove(trace_path);


		auto cmd = std::format("tar -xf {0} -C {1}", file_path.string(), extra_dir.string());
		system(cmd.c_str());
	}
	else
	{
		trace_path = path;
	}


	auto trace_file = std::fstream(trace_path,std::ios::in | std::ios::binary);
	if (!trace_file)
		return false;

	auto file_size = std::filesystem::file_size(trace_path);
	std::string file_content(file_size,0);
	trace_file.read(file_content.data(), file_size);


	char* cur_pos = file_content.data();
	char* end_pos = file_content.data() + file_size;
	auto read = [&](auto& val){
		if (cur_pos + sizeof(val) > end_pos)
		{
			Error("out of range");
			return false;
		}

		memcpy(&val, cur_pos, sizeof(val));
		cur_pos += sizeof(val);
		return true;
		//trace_file.read((char*) & val, sizeof(val));
		//return trace_file.good() && !trace_file.eof();
	};

	auto read_string = [&](){
		uint32_t len;
		read(len);
		std::string str;
		if (len > 0)
		{
			str.resize(len);
			memcpy(&str[0], cur_pos, len);
			cur_pos += len;
			//trace_file.read(&str[0], len);
		}
		return str;
	};

	auto read_data = [&](char* data, auto size)
	{
		if (cur_pos + size > end_pos)
		{
			Error("out of range");
			return false;
		}
		memcpy(data, cur_pos, size);
		cur_pos += size;
		return true;
	};

	auto skip = [&](auto size){
		if (cur_pos + size > end_pos)
		{
			Error("out of range");
			return false;
		}

		cur_pos += size;
		return true;
	};


	uint32_t version = 0;
	read(version);
	

	uint32_t stack_len = 0;
	read(stack_len);

	std::vector<int> node_map;
	node_map.reserve(stack_len);

	Calltree::Ptr calltree = Calltree::create(stack_len);
	uint32_t cur_depth = 0;

	read(cur_depth);
	calltree->root->name = read_string();
	node_map.push_back(0);

	auto parent = calltree->root;

	Node* cur = parent;
	int node_count = 1;
	for (int i = 1; ; ++i)
	{
		uint32_t depth;
		if (!read(depth) || depth == -1)
		{
			assert(i == stack_len);
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
			}
			while (depth - 1 != cur_depth);
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

		cur = new ((*calltree)[node_count])Node();
		node_map.push_back(node_count);
		cur->name = read_string();
		cur->parent = {calltree,calltree->getIndex(parent)};
		parent->children.emplace_back(calltree, node_count);

		node_count++;
	}

	Assert(node_count == stack_len, "invalid node count, sum {}  record {}", node_count, stack_len);

	std::vector<AllocInfo> allocs;

	uint32_t alloc_count;
	read(alloc_count);
	allocs.reserve(alloc_count);
	struct InternalAllocInfo
	{
		uint32_t size;
		uint32_t index;
		uint32_t start;
		uint32_t end;

		uint32_t rhi_ref;
	};

	uint32_t start = -1;
	uint32_t end = 1;
	std::vector<uint32_t> zero_end;
	zero_end.reserve(alloc_count);

	std::unordered_map<uint64_t, std::vector<uint32_t>> node_indices;

	for (int i = 0; i < alloc_count; ++i)
	{
		AllocInfo ai;

		uint32_t index;

		read(ai.size);
		read(index);
		read(ai.start);
		read(ai.end);

		if (version >= 8)
		{
			read(ai.user_tag);
		}

		if (ai.end == 0)
			zero_end.push_back((uint32_t)allocs.size());
		else
			ai.end = ai.end;


		start = std::min(start, ai.start);
		end = std::max(end, ai.end);
		end = std::max(end, ai.start);


		ai.node = {calltree,node_map[index]};

		allocs.push_back(ai);
	}


	for (auto idx : zero_end)
	{
		allocs[idx].end = end;
	}



	zero_end.clear();


	int num_snapshots = 0;
	read(num_snapshots);
	std::vector< Checkpoint> chkpnts;
	for (int i = 0; i < num_snapshots; ++i)
	{
		Checkpoint cp;
		read(cp.frameid);
		cp.text = read_string();
		chkpnts.push_back(cp);
	}

	std::vector<TotalInfo> totals;
	std::vector<ObjectInfo> objects;
	if (version >= 2)
	{
		// frame info
		uint32_t num_frame = 0;
		read(num_frame);

		totals.reserve(num_frame);
		for (uint32_t i = 0; i < num_frame; ++i)
		{
			TotalInfo info;
			read(info.used);
			read(info.available);
			if (version > 2)
				read(info.overhead);

			if (version >= 7)
			{
				int count;
				read(count);
				for (int j = 0; j < count; ++j)
				{
					auto key = read_string();
					uint32_t value;
					read(value);
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
			read(num_class);


			std::map<int, ObjClassInfo::Ptr> obj_class_map;
			for (uint32_t i = 0; i < num_class; ++i)
			{
				auto info = std::make_shared<ObjClassInfo>();

				int count = 0;
				read(count);
				for (int j = 0; j < count; ++j)
				{
					info->name_chain.push_back(read_string());
				}

				std::reverse(info->name_chain.begin(), info->name_chain.end());

				obj_class_map.emplace(i, info);
			}

			uint32_t num_objects = 0;
			read(num_objects);

			for (uint32_t i = 0; i < num_objects; ++i)
			{
				ObjectInfo info;
				info.name = read_string();
				int class_id;
				read(class_id);

				info.class_chain = obj_class_map[class_id];

				int node_index;
				read(node_index);

				info.node = {calltree,node_index};
				read(info.begin);
				read(info.end);
				if (info.end == 0)
					info.end = end;

				objects.push_back(std::move(info));
			}
		}
		else
		{
			uint32_t num_objects = 0;
			read(num_objects);

			for (uint32_t i = 0; i < num_objects; ++i)
			{
				read_string();
				read_string();
				int num;
				read(num);
				read(num);
				read(num);
				read(num);

				int count;
				read(count);
				for (int j = 0; j < count; ++j)
				{
					auto key = read_string();
					auto value = read_string();
				}
			}
		}

	}

	// rhi info
	std::vector<RhiInfo> rhis;
	if (version >= 3)
	{
		uint32_t num_rhi = 0;
		read(num_rhi);
		rhis.reserve(num_rhi);
		for (uint32_t i = 0; i < num_rhi; ++i)
		{
			RhiInfo info;
			uint64_t ptr;
			read(ptr);
			info.ptr = ptr;
			info.name = read_string();
			read(info.begin);
			read(info.end);
			read(info.node_index);
			uint32_t attr;
			read(info.type);
			read(info.size);
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
					read(attr);
					info.attrs.push_back(attr);
				}

				break;
			default:
				std::cout << std::format("failed to parse rhi on index : {}\n", i);
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
		read(num_lua_node);

		lua_calltree = Calltree::create(num_lua_node);
		uint32_t cur_depth = 0;

		read(cur_depth);
		lua_calltree->root->name = read_string();

		auto parent = lua_calltree->root;

		Node* cur = parent;
		int node_count = 1;
		for (int i = 1; ; ++i)
		{
			uint32_t depth;
			if (!read(depth) || depth == -1)
			{
				assert(i == num_lua_node);
				break;
			}

			assert(depth < 1024);

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

			cur = new ((*lua_calltree)[node_count]) Node();
			cur->name = read_string();
			cur->parent = {lua_calltree, lua_calltree->getIndex(parent)};
			parent->children.emplace_back(lua_calltree, node_count);

			node_count++;
		}
		

		uint32_t num_lua_alloc = 0;
		read(num_lua_alloc);
		for (int i = 0; i < num_lua_alloc; ++i)
		{
			AllocInfo ai;

			uint32_t index;

			read(ai.size);
			read(index);
			read(ai.start);
			read(ai.end);

			if (ai.end == 0)
				zero_end.push_back((uint32_t)lua_allocs.size());
			else
				ai.end = ai.end;


			start = std::min(start, ai.start);
			end = std::max(end, ai.end);
			end = std::max(end, ai.start);

			Assert(index < num_lua_node, "invalid index");
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
		read(num_objs);
		std::vector<char> buffer;
		for (uint32_t i = 0; i < num_objs; ++i)
		{
			int type;
			read(type);
			auto data = CustomData::createCustomData(type);
			data->type = type;
			data->name = read_string();
			read(data->size);
			read(data->begin);
			read(data->end);
			uint32_t size;
			read(size);
			if (size > 0)
			{
				buffer.resize(size);
				read_data(buffer.data(), size);

				data->serialize(buffer.data(), size);
			}
			customs.push_back(data);
		}
	}

	{
		trace->lua_calltree = lua_calltree;
		trace->lua_allocs = std::move(lua_allocs);
		//Data.node_count = num_lua_node;

		trace->calltree = calltree;
		trace->allocs = std::move(allocs);
		//Data.node_count = stack_len;

		trace->memoryinfos = std::move(totals);
		trace->objects = std::move(objects);
		trace->rhis = std::move(rhis);
		trace->checkpoints = std::move(chkpnts);
		trace->max_range.begin = start;
		trace->max_range.end = end;
		trace->customs = std::move(customs);

	}




	//InitializeFrameParser();

	return true;
}


#include <Windows.h>
bool TraceData::ParseTraceSpanFile(const std::string& path, TraceData* trace)
{
	auto work_dir = std::filesystem::current_path();
	auto extra_dir = work_dir / "caches";

	std::filesystem::path file_path = path;
	auto cmd = std::format("tar -xf {} -C {}", file_path.string(),extra_dir.string());
	run_cmd(cmd);

	auto file = std::fstream(extra_dir /"upload",std::ios::in | std::ios::binary);
	auto file_size = std::filesystem::file_size(extra_dir / "upload");
	std::vector<char> buffer;
	buffer.resize(file_size);

	file.read(buffer.data(), file_size);

	Assert(file.good(), "read file failed");
	auto cur = buffer.data();
	auto end = cur + file_size;


	TraceData::SerializeTraceSpan(trace, [&](uint8_t* buffer, int size) {
		memcpy(buffer, cur, size);
		cur+= size;
		Assert( cur <= end,"invalid reading");
	}, false);

	//auto read = [&](auto& val){
	//	memcpy(&val, cur, sizeof(val));
	//	cur += sizeof(val);
	//	Assert( cur <= end,"invalid reading");
	//	//file.read((char*) & val, sizeof(val));
	//};

	//auto read_string= [&](){
	//	int len ;
	//	read(len);
	//	std::string str;
	//	str.resize(len);
	//	//file.read(str.data(), len);
	//	memcpy(str.data(), cur, len);
	//	cur += len;
	//	Assert(cur <= end, "invalid reading");
	//	return str;
	//};

	//int version;
	//read(version);

	//int num_node;
	//read(num_node);
	//auto tree = Calltree::create(num_node);

	//for (int i = 1; i < num_node; ++i)
	//{
	//	auto node = new (tree->root + i)Node();

	//	int parent_idx;
	//	read(parent_idx);
	//	Assert(parent_idx < i, "invalid parent index: %d", parent_idx);

	//	node->name = read_string();
	//	read(node->basic_size);
	//	read(node->basic_count);

	//	node->parent = {tree, parent_idx};
	//	node->parent->children.emplace_back(tree, i);
	//}
	//trace->calltree = tree;


	//uint32_t num_rhis = 0;
	//read(num_rhis);
	//for (uint32_t i = 0; i < num_rhis; ++i)
	//{
	//	RhiInfo rhi;
	//	read(rhi.ptr);
	//	read(rhi.type);
	//	read(rhi.size);
	//	read(rhi.begin);
	//	read(rhi.end);
	//	rhi.name = read_string();

	//	if (rhi.type < RhiInfo::Vertex)
	//	{
	//		for (int j = 0; j < 6; ++j)
	//		{
	//			int val;
	//			read(val);
	//			rhi.attrs.push_back(val);
	//		}
	//	}
	//	trace->rhis.push_back(std::move(rhi));
	//}


	return true;
}


bool TraceData::SerializeTraceSpan(TraceData* trace, std::function<void(uint8_t*, int)>&& serialize,const bool saving)
{
	const bool loading = !saving;
	auto fmt = [&](auto& val){
		serialize((uint8_t*)&val, sizeof(val));
	};

	auto fmt_str = [&](std::string& str){
		int len = str.length();
		fmt(len);
		str.resize(len);
		serialize((uint8_t*)str.data(), len);
	};


	int version = 1;
	fmt(version);

	uint32_t num_node;
	if (loading)
	{
		fmt(num_node);
		trace->calltree = Calltree::create(num_node);
	}
	else
	{
		num_node = trace->calltree->node_count;
		fmt(num_node);
	}

	auto tree = trace->calltree;

	for (uint32_t i = 1; i < num_node; ++i)
	{
		Node* node;
		if (loading)
			node = new (tree->root + i)Node();
		else
			node = tree->root + i;

		int parent_idx = node->parent.index;
		fmt(parent_idx);
		//Assert(parent_idx < i, "invalid parent index: %d", parent_idx);

		fmt_str(node->name);

		fmt(node->basic_size);
		fmt(node->basic_count);

		if (loading)
		{
			node->parent = { tree, parent_idx };
		}
	}

	if (loading)
	{
		for (uint32_t i = 1; i < num_node; ++i)
		{
			auto node = tree->root + i;
			node->parent->children.emplace_back(tree, i);
		}
	}

	uint32_t num_rhis = trace->rhis.size();
	fmt(num_rhis);
	auto& rhis = trace->rhis;
	rhis.reserve(num_rhis);
	for (uint32_t i = 0; i < num_rhis; ++i)
	{
		RhiInfo* rhi;
		if (loading)
		{
			rhis.emplace_back();
		}
		rhi = &rhis[i];
		fmt(rhi->ptr);
		fmt(rhi->type);
		fmt(rhi->size);
		fmt(rhi->begin);
		fmt(rhi->end);
		fmt_str(rhi->name);

		if (rhi->type < RhiInfo::Vertex)
		{
			if (loading)
			{
				rhi->attrs.resize(6);
			}
			for (auto& i : rhi->attrs)
			{
				fmt(i);
			}
		}
	}




	uint32_t num_objs = trace->objects.size();
	fmt(num_objs);
	auto& objs = trace->objects;
	objs.reserve(num_objs);
	std::vector<ObjClassInfo::Ptr> class_infos;
	std::vector<int> indices;
	if (saving)
	{
		std::map<ObjClassInfo::Ptr, int> class_mapping;
		indices.resize(num_objs);

		for (uint32_t i = 0; i < num_objs; ++i)
		{
			auto& obj = objs[i];
			auto ret = class_mapping.find(obj.class_chain);
			if (ret != class_mapping.end())
			{
				indices[i] = ret->second; 
			}
			else
			{
				class_mapping.emplace(obj.class_chain, (int)class_infos.size());
				class_infos.push_back(obj.class_chain);
			}
		}
	}

	uint32_t num_class = class_infos.size();
	fmt(num_class);
	class_infos.reserve(num_class);
	for (uint32_t i = 0; i < num_class; ++i)
	{
		ObjClassInfo::Ptr cls;
		if (loading)
		{
			cls = std::make_shared<ObjClassInfo>();
			class_infos.push_back(cls);
		}
		else
		{
			cls = class_infos[i];
		}

		int count = cls->name_chain.size();
		fmt(count);
		if (loading)
		{
			cls->name_chain.resize(count);
		}
		for (auto& n : cls->name_chain)
			fmt_str(n);
	}


	for (uint32_t i = 0; i < num_objs; ++i)
	{
		if (loading)
		{
			objs.emplace_back();
		}

		auto& obj = objs[i];
		fmt_str(obj.name);
		int node_index = obj.node.index;
		fmt(node_index);
		obj.node = {tree, node_index};
		
		int class_index = 0;
		if (saving)
		{
			class_index = indices[i];
		}
		fmt(class_index);
		obj.class_chain = class_infos[class_index];
		fmt(obj.begin);
		fmt(obj.end);
	}


	return true;
}


Node* NodeRef::getNode()const
{
	if (tree)
		return (*tree)[index];
	else
		return nullptr;
}

NodeRef::NodeRef(std::shared_ptr<Calltree> parent, Node* node)
{
	index = parent->getIndex(node);
}


Calltree::Ptr Calltree::clone()const
{

	auto tree = Calltree::create(node_count);


	for (int i = 0; i < node_count; ++i)
	{
		new (tree->root + i)Node();

		tree->root[i] = root[i];
		tree->root[i].parent << tree;

		for (auto& c : tree->root[i].children)
		{
			c << tree;
		}

	}

	tree->root[0].parent = {};

	return tree;
}

Calltree::Ptr Calltree::diff(Ptr tree1, Ptr tree2)
{
	auto tree = create(tree1->node_count + tree2->node_count);

	new (tree->root)Node();
	tree->root[0].parent = {};
	tree->root[0].name = "root";
	int cur = 1;
	auto create_node = [&](){
		new (tree->root + cur)Node();
		return NodeRef{tree, cur++};
	};
	auto merge_node = [&](Node* n1, Node* n2, NodeRef node,bool reversed,auto& merge_fuc)->void{
		

		if (n1->name.find("MeshCluster::Resize") != -1)
		{
			Log("err");
		}

		std::stable_sort(n1->children.begin(), n1->children.end(), [](auto& a, auto& b){
			return a->name < b->name;
		});

		if (n2)
		{
			std::stable_sort(n2->children.begin(), n2->children.end(), [](auto& a, auto& b) {
				return a->name < b->name;
			});
		}

		std::vector<NodeRef>::iterator beg1,beg2,end1,end2;
		if (n2)
		{
			if (n1->children.size() < n2->children.size())
			{
				beg1 = n2->children.begin();
				end1 = n2->children.end();

				beg2 = n1->children.begin();
				end2 = n1->children.end();
				reversed = !reversed;
			}
			else
			{
				beg1 = n1->children.begin();
				end1 = n1->children.end();

				beg2 = n2->children.begin();
				end2 = n2->children.end();
			}

		}
		else
		{
			beg1 = n1->children.begin();
			end1 = n1->children.end();

			beg2 = n1->children.end();
			end2 = n1->children.end();
		}

		while (beg1 != end1 || beg2 != end2)
		{
			int cmp = 0;
			if (beg1 != end1 && beg2 != end2)
			{
				auto& n1 = (*beg1)->name;
				auto& n2 = (*beg2)->name;

				auto ret = n1 <=> n2;

				if (ret == std::strong_ordering::less)
				{
					cmp = -1;
				}
				else if(ret == std::strong_ordering::equal)
				{
					cmp = 0;
				}
				else
				{
					cmp = 1;
				}
			}

			if ((beg1 != end1 && beg2 == end2) || cmp < 0)
			{
				auto n = create_node();
				if (!reversed)
				{
					n->parent = node;
					n->basic_size = -(*beg1)->basic_size;
					n->basic_count = -(*beg1)->basic_count;
					n->name = (*beg1)->name;
					node->children.push_back(n);



					merge_fuc(**beg1, nullptr, n, false, merge_fuc);
				}
				else
				{
					n->parent = node;
					n->basic_size = (*beg1)->basic_size;
					n->basic_count = (*beg1)->basic_count;
					n->name = (*beg1)->name;
					node->children.push_back(n);



					merge_fuc(**beg1, nullptr, n, true, merge_fuc);
				}

				++beg1;
			}
			else if ((beg1 != end1 && beg2 != end2) && cmp == 0 )
			{
				auto n = create_node();
				if (!reversed)
				{
					n->parent = node;
					n->basic_size = -(*beg1)->basic_size;
					n->basic_count = -(*beg1)->basic_count;
					n->name = (*beg1)->name;
					node->children.push_back(n);

					n->basic_size += (*beg2)->basic_size;
					n->basic_count += (*beg2)->basic_count;

					merge_fuc(**beg1, **beg2, n, false, merge_fuc);
				}
				else
				{
					n->parent = node;
					n->basic_size = (*beg1)->basic_size;
					n->basic_count = (*beg1)->basic_count;
					n->name = (*beg1)->name;
					node->children.push_back(n);

					n->basic_size -= (*beg2)->basic_size;
					n->basic_count -= (*beg2)->basic_count;

					merge_fuc(**beg1, **beg2, n, true, merge_fuc);
				}


				++beg1;
				++beg2;
			}
			else if ((beg2 != end2 && beg1 == end1) || cmp > 0)
			{
				auto n = create_node();
				if (reversed)
				{
					n->parent = node;
					n->basic_size = -(*beg2)->basic_size;
					n->basic_count = -(*beg2)->basic_count;
					n->name = (*beg2)->name;
					node->children.push_back(n);



					merge_fuc(**beg2, nullptr, n, false, merge_fuc);
				}
				else
				{
					n->parent = node;
					n->basic_size = (*beg2)->basic_size;
					n->basic_count = (*beg2)->basic_count;
					n->name = (*beg2)->name;
					node->children.push_back(n);

					merge_fuc(**beg2, nullptr, n, true, merge_fuc);
				}

				++beg2;
			}
			else
			{
				Error("err");
			}

		}


		node->size = node->basic_size;
		node->count = node->basic_count;
		
	};

	merge_node(tree1->root, tree2->root, NodeRef{ tree, 0 }, false, merge_node);
	tree->node_count = cur;
	return tree;
}

