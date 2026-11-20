#include "CoreTypes.h"
#include "Utils.h"
#include <fstream>
#include <filesystem>
#include <format>
#include "TraceInstance.h"

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
