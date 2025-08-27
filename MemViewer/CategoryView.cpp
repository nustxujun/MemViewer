#include "CategoryView.h"
#include "MemViewer.h"
#include "Utils.h"
#include "TraceParser.h"
#include "FrameParser.h"
#include "TraceInstance.h"
#include "Concurrency.h"

#include "imgui/imgui.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <Windows.h>

#include "resource.h"

#undef min
#undef max

uint32_t CategoryView::GetUntagged(const std::vector<Category>& cats)
{
	uint32_t cat_idx = 0;
	for (auto& cat : cats)
	{
		if (cat.name == "Untagged")
			return cat_idx;
		cat_idx++;
	}
	return -1;
}

uint32_t CategoryView::MatchCategory(const std::string& str, const std::vector<CategoryView::Category>& cats)
{
	uint32_t cat_idx = 0;
	for (auto& cat : cats)
	{
		for (auto& filter : cat.filters)
		{
			if (str.find(filter) == std::string::npos)
				continue;
			return cat_idx;
		}
		cat_idx++;
	}
	return -1;
}

uint32_t CategoryView::findCategory()
{

	auto trace = GetTrace();
	auto node_count = trace->getCalltree()->node_count;

	auto calltree = trace->getCalltree();

	struct InternalAllcInfo
	{
		uint64_t size = 0;
		uint32_t count = 0;
		int category;
		uint32_t matched_node_index;
	};


	int untaggedid = GetUntagged(categories);
	auto thread_count = std::thread::hardware_concurrency() + 1;
	auto stride = ALIGN(node_count, thread_count) / thread_count;

	std::unordered_map<uint32_t, InternalAllcInfo> matched_map;
	std::mutex mutex;

	ParallelTask([&](int i) {
		size_t begin = i * stride;
		size_t end = std::min((size_t)(i + 1) * stride, (size_t)node_count);

		for (auto i = begin; i < end; ++i)
		{
			auto node = calltree->get(i);
			int cat_idx = -1;
			NodeRef matched_node;
			NodeRef noderef = {calltree, (int)i};


			auto cur_node = node;
			while (cur_node)
			{
				EXIT_IF_STOPED();

				cat_idx = MatchCategory(cur_node->name, categories);
				if (cat_idx != -1)
				{
					break;
				}
				cur_node = *cur_node->parent;
			}
			if (cur_node != nullptr)
				matched_node = NodeRef{calltree, cur_node };
			if (cat_idx == -1)
			{
				cat_idx = untaggedid;
			}

			if (node->basic_size == 0)
				continue;
			std::lock_guard lock(mutex);
			categories[cat_idx].total_size += node->basic_size;
			alloc_infos[categories[cat_idx].index].push_back({ noderef, matched_node, node->basic_count, node->basic_size});
		}

		}, thread_count);


	return 0;
}

void CategoryView::InitializeImpl()
{
	Counter counter("init category");

	alloc_infos.clear();
	//timeline_datas.clear();
	sub_categories.clear();

	selected_category = 0;
	selected_sub_category = 0;
	selected_symbol = 0;
	show_level = 0;
	categories.clear();
	symbol_filters.clear();


	auto trace = GetTrace();
	{
		std::string path;
		if (selected_config_file == 0)
			path = get_or_create_default_file(IDR_CATEGORY_CONFIG_INI, "category_config.ini");
		else
			path = get_or_create_default_file(IDR_LLM_CONFIG_INI, "llm_config.ini");

		auto f = std::fstream(path, std::ios::in);
		std::string content;
		std::vector<char> temp;
		temp.resize(1024 * 1024);
		while (!f.eof() && f.good())
		{
			
			f.getline(temp.data(), 1024 * 1024);
			std::regex pattern("([^=]+)=?([^=]*)");
			std::smatch results;
			std::string content = temp.data();
			const bool is_sub = temp[0] == '+';
			if (std::regex_search(content, results, pattern))
			{
				if (is_sub)
				{
					auto parent = categories.rbegin()->index;
					sub_categories[parent].push_back({ results[1].str().substr(1), split_string(results[2],"|"),0,(int)sub_categories[parent].size() });
				}
				else
				{	
					categories.push_back({ results[1], split_string(results[2],"|"), 0,(int)categories.size()});
					sub_categories.resize(categories.size());

				}
			}

		}

	}

	if (categories.rbegin()->name == "Untagged")
		categories.rbegin()->filters.clear();
	else
		categories.push_back({ "Untagged", {} ,0,int(categories.size())});
		
	sub_categories.resize(categories.size());
	alloc_infos.resize(categories.size());
	//timeline_datas.resize(categories.size());

	for (auto& c : categories)
	{
		sub_categories[c.index].push_back({ "Untagged", {}, 0, int(sub_categories[c.index].size())});
	}



	std::vector<std::vector<int64_t>> total_sizes;
	total_sizes.resize(categories.size());


	//auto calltree = trace->getCalltree();
	//auto count = trace->getTraceRange().count() + 1;

	//for (auto& vec : total_sizes)
	//{
	//	vec.resize(count);
	//}


	//auto alloc_count = trace->getTotalAllocs().size();
	//auto pipline = std::thread::hardware_concurrency() + 1;
	//auto stride = ALIGN(alloc_count, pipline) / pipline;
	//std::mutex mutex;

	//ParallelTask([&](int begin) {

	//	std::vector<std::vector<int64_t>> sizes;
	//	sizes.resize(categories.size());

	//	for (auto& vec : sizes)
	//		vec.resize(count);

	//	auto end = std::min(alloc_count, (begin + 1) * stride);
	//	std::unordered_map<uint32_t, int> matched_map;
	//	for (int i = begin * stride; i < end; ++i)
	//	{
	//		EXIT_IF_STOPED()

	//		auto& alloc = trace->getTotalAllocs()[i];
	//		int cat_idx = -1;
	//		auto is_matched = matched_map.find(alloc.node.index);
	//		if (is_matched == matched_map.end())
	//		{
	//			auto cur_node = *alloc.node;
	//			while (cur_node)
	//			{
	//				cat_idx = MatchCategory(cur_node->name, categories);
	//				if (cat_idx != -1)
	//				{
	//					break;
	//				}
	//				cur_node = *cur_node->parent;
	//			}
	//			matched_map[alloc.node.index] = cat_idx;
	//		}
	//		else
	//		{
	//			cat_idx = is_matched->second;
	//		}

	//		if (cat_idx == -1)
	//		{
	//			cat_idx = sizes.size() - 1;
	//		}
	//		{
	//			auto& size_vec = sizes[cat_idx];
	//			auto end = std::min(alloc.end,trace->getTraceRange().end);
	//			for (int i = alloc.start; i ALLOC_END_CMP(<= , < ) end; ++i)
	//			{
	//				size_vec[i - trace->getTraceRange().begin] += alloc.size;
	//			}
	//		}
	//	}


	//	std::lock_guard lock(mutex);
	//	for (auto i = 0; i < categories.size(); ++i)
	//	{
	//		auto& src_vec = sizes[i];
	//		auto& dst_vec = total_sizes[i];
	//		for (auto j = 0; j < count; ++j)
	//		{
	//			dst_vec[j] += src_vec[j];
	//		}
	//	}

	//	}, pipline);




	//float inv = 1.0f / 1024.0f / 1024.0f;
	//int cur_idx = 0;
	//for (auto vec : total_sizes)
	//{
	//	EXIT_IF_STOPED()

	//	auto& data_vec = timeline_datas[categories[cur_idx++].index];
	//	data_vec.reserve(count);
	//	for (auto size : vec)
	//	{
	//		auto total_size = (size)*inv;
	//		data_vec.push_back(total_size);
	//	}
	//}

}
void CategoryView::UpdateImpl()
{
	if (!GetTrace()->getCalltree())
		return;
	Counter counter("update category");
	for (auto& c : categories)
		c.total_size = 0;
	for (auto& infos : alloc_infos)
		infos.clear();
	for (auto& cat : categories)
		alloc_infos[cat.index] = {};

	findCategory();

	auto count = categories.size();

	ParallelTask([&](int i) {
		auto& alloc = alloc_infos[categories[i].index];
		std::stable_sort(alloc.begin(), alloc.end(), [](auto& a, auto& b) {
			return a.size > b.size;
			});

		}, count);

	//for (auto& alloc : alloc_infos)
	//{
	//	std::stable_sort(alloc.second.begin(), alloc.second.end(), [](auto& a, auto& b){
	//		return a.size > b.size;
	//	});
	//}
	std::stable_sort(categories.begin(), categories.end(), [](auto& a, auto& b) {
		return a.total_size > b.total_size;
		});

	selected_symbol = 0;
	selected_category = 0;
	selected_sub_category = 0;
	UpdateSubCategory();
}


void CategoryView::UpdateSubCategory()
{
	if (categories.size() == 0)
		return;
	auto parent = categories[selected_category].index;
	auto& sub_cats = sub_categories[parent];
	for (auto& c : sub_cats)
	{
		c.total_size = 0;
	}

	sub_alloc_infos.clear();
	sub_alloc_infos.resize(sub_cats.size());
	auto calltree = GetTrace()->getCalltree();

	int untagged_id = GetUntagged(sub_cats);

	for (auto& alloc : alloc_infos[parent])
	{
		auto node =  alloc.node_index;
		auto cur_node = node;
		int idx = -1;
		while (cur_node)
		{
			idx = MatchCategory(cur_node->name, sub_cats);
			if (idx != -1)
			{
				break;
			}

			cur_node = cur_node->parent;
		}

		if (idx == -1)
		{
			idx = untagged_id;
		}
		sub_cats[idx].total_size += alloc.size;
		sub_alloc_infos[sub_cats[idx].index].push_back({ alloc.node_index,cur_node ? cur_node : alloc.matched_node_index,alloc.count, alloc.size });
	}

	std::stable_sort(sub_cats.begin(), sub_cats.end(), [](auto& a, auto& b) {
		return a.total_size > b.total_size;
		});
}


void CategoryView::MakeCategory()
{
	uint32_t idx = 0;
	for (auto& cat : categories)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Selectable(cat.name.c_str(), selected_category == idx, ImGuiSelectableFlags_SpanAllColumns))
		{
			if (selected_category != idx)
			{
				selected_category = idx;

				selected_sub_category = 0;
				selected_symbol = 0;
				UpdateSubCategory();
				//UpdateTimeline(timeline_datas[cat.index]);
			}
		}
		ImGui::TableNextColumn();
		ImGui::Text(size_tostring(cat.total_size).c_str());

		idx++;
	}
}


void CategoryView::MakeSubCategory()
{
	uint32_t idx = 0;
	for (auto& cat : sub_categories[categories[selected_category].index])
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Selectable(cat.name.c_str(), selected_sub_category == idx, ImGuiSelectableFlags_SpanAllColumns))
		{
			if (selected_sub_category != idx)
			{
				selected_sub_category = idx;
				selected_symbol = 0;
			}
		}
		ImGui::TableNextColumn();
		ImGui::Text(size_tostring(cat.total_size).c_str());

		idx++;
	}
}

void CategoryView::MakeSymbols()
{
	auto calltree = GetTrace()->getCalltree();
	uint32_t idx = 0;
	for (auto& alloc : sub_alloc_infos[sub_categories[categories[selected_category].index][selected_sub_category].index])
	{
		auto node = *alloc.node_index;

		auto bdraw = symbol_filters.empty() || [&]() {
			auto cur_node = node;
			while (cur_node)
			{
				//auto name = to_lower(cur_node->name);
				for (auto& filter : symbol_filters)
				{
					if (cur_node->name.find(filter) == std::string::npos)
						continue;
					return true;
				}
				cur_node = *cur_node->parent;
			}

			return false;
		}();

		if (bdraw)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(node);

			auto show_node = node;
			for (int i = 0; i < show_level && show_node->parent; ++i)
			{
				show_node = *show_node->parent;
			}

			if (ImGui::Selectable(show_node->name.c_str(), selected_symbol == idx, ImGuiSelectableFlags_SpanAllColumns))
			{
				selected_symbol = idx;
			}
			if (ImGui::BeginItemTooltip())
			{
				ImGui::TextColored(ImVec4(1, 1, 0, 1), show_node->name.c_str());
				ImGui::EndTooltip();
			}
			//ImGui::SetItemTooltip(node->name.c_str());

			ImGui::TableNextColumn();
			ImGui::Text(size_tostring(alloc.size).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%d", alloc.count);
			ImGui::PopID();
		}
		idx++;
	}
}


void CategoryView::MakeStack()
{

	auto& info = sub_alloc_infos[sub_categories[categories[selected_category].index][selected_sub_category].index][selected_symbol];
	auto node = info.node_index;


	auto cur_node = node;
	while (cur_node)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::PushID(cur_node);
		bool matched = cur_node == info.matched_node_index;
		bool is_colored = matched;
		if (matched)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0.5, 1));
		}
		else
		{
			for (auto& filter : symbol_filters)
			{
				if (cur_node->name.find(filter) == std::string::npos)
					continue;
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 1, 0.5, 1));
				is_colored = true;
			}
		}
		ImGui::PushID(*cur_node);
		ImGui::Selectable(cur_node->name.c_str());
		ImGui::PopID();
		if (is_colored)
			ImGui::PopStyleColor();
		if (ImGui::BeginItemTooltip())
		{
			ImGui::TextColored(ImVec4(1, 1, 0, 1), cur_node->name.c_str());
			ImGui::EndTooltip();
		}

		ImGui::PopID();
		cur_node = cur_node->parent;
	}
}

void CategoryView::ShowImpl()
{
	const ImGuiChildFlags child_flags = ImGuiChildFlags_ResizeX;
	const ImGuiTableFlags tbl_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;


	//if (ImGui::Button(is_top_down ? "Top down" : "Bottom up"))
	//{
	//	is_top_down = !is_top_down;
	//}

	//ImGui::SameLine();

	const char* config_files[]={"category_config.ini","llm_config.ini"};
	auto selected = selected_config_file;
	ImGui::SetNextItemWidth(250);
	if (ImGui::Combo("", &selected, config_files, 2))
	{
		if (selected != selected_config_file)
		{
			selected_config_file = selected;
			Initialize();
			Update();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("open config file"))
	{
		::ShellExecuteA(0, 0, config_files[selected], 0, 0, SW_SHOW);
	}

	ImGui::SameLine();

	if (ImGui::Button("reload config"))
	{
		Initialize();
		Update();
	}

	ImGui::SameLine();

	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(
			R"(1. keywords are case-sensitive

2. use '|' to split different key word
example: Texture=CreateTexture|LogAllocTexture

3. use '+' to mark category as sub category, sub category will be searched from results of category
example:
	Physics=PhysX
	Texture=CreateTexture
	+RT=RenderTarget
	+Slate=Slate
	+Texture3D=Cube
)"
);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	int step = std::max(1, scaling / 10);
	ImGui::SetNextItemWidth(100);
	//if (ImGui::InputInt("scaling", &scaling, step))
	//{
	//	scaling = std::max(1, scaling);
	//	if (selected_category < categories.size())
	//		UpdateTimeline(timeline_datas[categories[selected_category].index]);
	//}


	ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody;
	if (ImGui::BeginTable("table", 4, flags))
	{
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);

		ImGui::BeginChild("category");
		if (ImGui::BeginTable("category", 2, tbl_flags))
		{
			ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100);
			ImGui::TableHeadersRow();


			if (categories.size() > 0)
			{
				MakeCategory();

			}

			ImGui::EndTable();
		}
		ImGui::EndChild();



		ImGui::TableSetColumnIndex(1);

		ImGui::BeginChild("subcategory");

		if (ImGui::BeginTable("subcategory", 2, tbl_flags))
		{
			ImGui::TableSetupColumn("Sub Category", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100);
			ImGui::TableHeadersRow();


			if (selected_category < categories.size())
			{
				MakeSubCategory();

			}

			ImGui::EndTable();
		}
		ImGui::EndChild();
		ImGui::TableSetColumnIndex(2);


		{

			ImGui::BeginChild("itemlist");


			if (ImGui::Button("+"))
			{
				show_level++;
			}
			ImGui::SameLine();
			if (ImGui::Button("-"))
			{
				show_level = std::max(0, show_level - 1);
			}
			ImGui::SameLine();

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 1, 0.5, 1));
			if (ImGui::InputTextWithHint("##filter", "search in callstack", temp, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				symbol_filters = split_string(temp, "|");
				selected_symbol = 0;
			}
			ImGui::PopStyleColor();
			ImGui::Text("show stack level %d", show_level);


			if (ImGui::BeginTable("Symbol", 3, tbl_flags))
			{
				ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_NoHide);
				ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100);
				ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 100);
				ImGui::TableHeadersRow();

				if (selected_category < categories.size())
				{
					auto cat_name = categories[selected_category].index;
					if (selected_sub_category < sub_categories[cat_name].size() && selected_category < alloc_infos.size() && alloc_infos[cat_name].size() > 0)
					{
						MakeSymbols();
					}
				}

				ImGui::EndTable();
			}

			ImGui::EndChild();
			ImGui::TableSetColumnIndex(3);
			//ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
			//ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

			ImGui::BeginChild("stack");
			if (ImGui::BeginTable("stack", 1, tbl_flags))
			{
				ImGui::TableSetupColumn("Callstack", ImGuiTableColumnFlags_NoHide);
				ImGui::TableHeadersRow();

				if (selected_category < categories.size())
				{
					auto cat_name = categories[selected_category].index;

					if (selected_sub_category < sub_categories[cat_name].size() && selected_category < alloc_infos.size() && alloc_infos[cat_name].size() > 0 &&
						selected_symbol < sub_alloc_infos[sub_categories[cat_name][selected_sub_category].index].size())
					{
						MakeStack();
					}
				}

				ImGui::EndTable();
			}
			ImGui::EndChild();
		}

		ImGui::EndTable();
	}
}


void CategoryView::UpdateTimeline(std::vector<float> timeline)
{
	std::transform(timeline.begin(), timeline.end(), timeline.begin(), [&](auto v) {return v * scaling; });
	//SetCustomData(std::move(timeline));
}
