#include "CustomObj.h"
#include "TraceParser.h"
#include "FrameParser.h"
#include "MemViewer.h"
#include "Utils.h"
#include "imgui/imgui.h"

void CustomObj::InitializeImpl() 
{
	
}

void CustomObj::UpdateImpl()
{
	auto range = GetFrameRange();
	auto& data = GetParsedData();

	auto begin_frame = std::min(range.first, range.second);
	auto end_frame = std::max(range.first, range.second);


	for (auto& obj: data.customs)
	{
		if (obj->begin > end_frame || obj->end < end_frame || obj->begin < begin_frame)
			continue;

		customs[obj->type].push_back(obj);
	}

	std::vector<std::string> types;
	for (auto& data : customs)
	{
		types.push_back(std::to_string(data.first));
	}

	auto& groups = categories.Init(types);


	for (auto& grp : groups)
	{
		grp.subs.push_back({"untagged"});
		auto& sub = grp.subs[0];
		
		auto type = atoi(grp.name.c_str());
		for (auto& data : customs[type])
		{
			sub.count += 1;
			sub.size += data->size;
			sub.datas.push_back({data->name,data->size, 1, data.get()});
		}

		grp.count += sub.count;
		grp.size += sub.size;
	}


	categories.Update([&](void* data){
	},
	[](const auto& name, auto){
	},
	[&](bool passed_by_name, void* data, const std::string& key)-> bool
	{
		return passed_by_name;
	});
}

void CustomObj::ShowImpl()
{
	categories.Show();
}