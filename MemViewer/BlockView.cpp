//#include "BlockView.h"
//#include "TraceParser.h"
//#include "FrameParser.h"
//#include "Utils.h"
//#include "imgui/imgui.h"
//#include "MemViewer.h"
//
//
//std::vector<int>& GetBlocks()
//{
//	static std::vector<int> Blocks;
//	if (Blocks.empty())
//	{
//		int block = 1024 * 1024 * 64;
//
//		while (block)
//		{
//			Blocks.push_back(block);
//			block >>=1 ;
//		}
//	}
//	return Blocks;
//}
//#pragma optimize("",off)
//
//int get_index(int size) {
//
//	int poweroftwo = 1;
//
//	int idx = 0;
//	while ((poweroftwo <<idx) < size)
//	{
//		idx++;
//	}
//	return idx;
//};
//
//void BlockView::InitializeImpl()
//{
//	std::stable_sort(GetBlocks().begin(), GetBlocks().end(), [](auto& a, auto& b) {
//		return a < b;
//	});
//
//
//	auto& data = GetParsedData();
//
//	timeline_data.resize(GetBlocks().size());
//	for (auto& list : timeline_data)
//	{
//		EXIT_IF_STOPED()
//		list.resize(data.frame_end - data.frame_begin + 1, 0);
//	}
//
//	float max_count = 0;
//	for (auto& alloc : data.allocs)
//	{
//		EXIT_IF_STOPED()
//
//		auto idx = get_index(alloc.size);
//		if (idx >= GetBlocks().size())
//			continue;
//		auto& list = timeline_data[idx];
//		//list[alloc.start - data.frame_begin] ++;
//		auto end = std::min(alloc.end - data.frame_begin, data.frame_end - data.frame_begin );
//		for (auto i = alloc.start - data.frame_begin; i < end; ++i)
//		{
//			list[i] ++;
//			max_count = std::max(list[i], max_count);
//		}
//		//max_count = std::max(list[alloc.start - data.frame_begin], max_count);
//
//	}
//
//	auto inv_max = 1.0f / max_count;
//	for (auto& list : timeline_data)
//	{
//		for (auto& count : list)
//		{
//			count *= inv_max;
//		}
//	}
//
//}
//void BlockView::UpdateImpl()
//{
//	auto& data = GetParsedData();
//
//	auto range = GetFrameRange();
//
//	allocs.clear();
//	infos.clear();
//	int idx = 0;
//	for (auto block_size : GetBlocks())
//	{
//		infos.push_back({ 0,0,block_size, idx++ });
//	}
//
//
//
//	std::vector<std::unordered_map<uint32_t, AllocInfo>> alloc_infos;
//	alloc_infos.resize(infos.size());
//
//
//	for (auto& alloc : data.allocs)
//	{
//		if ( alloc.start < range.first || alloc.start >= range.second)
//			continue;
//
//		if (alloc.end < range.second)
//			continue;
//
//
//
//		auto idx = get_index(alloc.size);
//		if (idx >= GetBlocks().size())
//			continue;
//		infos[idx].total_count ++;
//		infos[idx].total_size+= alloc.size;
//
//		auto& info = alloc_infos[idx][alloc.node_index];
//		info.count++;
//		info.node_index = alloc.node_index;
//		info.block_size = (idx + 1) * 16;
//		info.size += alloc.size;
//	}
//
//	for (auto& list : alloc_infos)
//	{
//		std::vector<AllocInfo> tmp;
//
//		for (auto& info : list)
//		{
//			tmp.push_back(info.second);
//		}
//
//		std::stable_sort(tmp.begin(), tmp.end(), [](auto& a, auto& b) {
//			return a.count > b.count;
//		});
//		allocs.push_back(std::move(tmp));
//	}
//
//	std::stable_sort(infos.begin(), infos.end(), [](auto& a, auto& b){
//		return a.total_count > b.total_count;
//	});
//}
//
//void BlockView::UpdateTimeline()
//{
//	if (timeline_data.size() == 0)
//		return;
//
//	auto tmp = timeline_data[infos[selected_block].index];
//	std::transform(tmp.begin(), tmp.end(), tmp.begin(), [&](auto v) {return v * scaling; });
//	SetCustomData2(std::move(tmp));
//}
//
//void BlockView::ShowImpl()
//{
//	const ImGuiTableFlags tbl_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
//
//
//	ImGui::BeginChild("blocks", ImVec2(500, 0), ImGuiChildFlags_ResizeX);
//
//	int step = std::max(1, scaling / 4);
//	if (ImGui::InputInt("scaling", &scaling, step, step * 100))
//	{
//		scaling = std::max(1, scaling);
//		UpdateTimeline();
//	}
//
//
//	if (ImGui::BeginTable("blocks", 3, tbl_flags))
//	{
//		ImGui::TableSetupColumn("BlockSize", ImGuiTableColumnFlags_NoHide);
//		ImGui::TableSetupColumn("TotalSize", ImGuiTableColumnFlags_NoHide);
//		ImGui::TableSetupColumn("TotalCount", ImGuiTableColumnFlags_WidthFixed, 150);
//		ImGui::TableHeadersRow();
//
//		if (infos.size() > 0)
//		{
//			MakeBlocks();
//		}
//		ImGui::EndTable();
//	}
//	ImGui::EndChild();
//	ImGui::SameLine();
//	ImGui::BeginChild("allocs", ImVec2(500, 0), ImGuiChildFlags_ResizeX);
//	if (ImGui::BeginTable("allocs", 3, tbl_flags))
//	{
//		ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_NoHide);
//		ImGui::TableSetupColumn("TotalSize", ImGuiTableColumnFlags_NoHide);
//		ImGui::TableSetupColumn("TotalCount", ImGuiTableColumnFlags_WidthFixed, 150);
//		ImGui::TableHeadersRow();
//		if (infos.size() > 0 && allocs[infos[selected_block].index].size() > 0)
//		{
//			MakeSymbols();
//		}
//		ImGui::EndTable();
//	}
//	ImGui::EndChild();
//	ImGui::SameLine();
//
//	if (ImGui::BeginTable("stacks", 1, tbl_flags))
//	{
//		ImGui::TableSetupColumn("##stack", ImGuiTableColumnFlags_NoHide);
//		ImGui::TableHeadersRow();
//		if (infos.size() > 0 && allocs[infos[selected_block].index].size() > 0)
//		{
//			MakeStacks();
//		}
//		ImGui::EndTable();
//	}
//}
//
//void BlockView::MakeBlocks()
//{
//	auto root = GetParsedData().root;
//	uint32_t idx = 0;
//	for (auto& info : infos)
//	{
//		ImGui::TableNextRow();
//		ImGui::TableNextColumn();
//		if (ImGui::Selectable(size_tostring(info.block_size).c_str(), selected_block == idx, ImGuiSelectableFlags_SpanAllColumns))
//		{
//			if (selected_block != idx)
//			{
//				selected_block = idx;
//				selected_symbol = 0;
//			}
//			UpdateTimeline();
//		}
//		ImGui::TableNextColumn();
//		ImGui::Text(size_tostring((uint64_t)info.total_size).c_str());
//		ImGui::TableNextColumn();
//		ImGui::Text("%d", info.total_count);
//
//
//		idx++;
//	}
//
//}
//
//void BlockView::MakeSymbols()
//{
//	auto root = GetParsedData().root;
//	uint32_t idx = 0;
//	uint64_t block_size = infos[selected_block].block_size;
//	auto& list = allocs[infos[selected_block].index];
//	for (auto& info : list)
//	{
//		auto node = root + info.node_index;
//		ImGui::TableNextRow();
//		ImGui::TableNextColumn();
//		ImGui::PushID(node);
//		if (ImGui::Selectable(node->name.c_str(), selected_symbol == idx, ImGuiSelectableFlags_SpanAllColumns))
//		{
//			if (selected_symbol != idx)
//			{
//				selected_symbol = idx;
//
//			}
//		}
//		ImGui::PopID();
//		ImGui::TableNextColumn();
//		ImGui::Text(size_tostring(info.size).c_str());
//		ImGui::TableNextColumn();
//		ImGui::Text("%d", info.count);
//		idx++;
//	}
//
//}
//
//void BlockView::MakeStacks()
//{
//	auto root = GetFrameData();
//
//	auto& info = allocs[infos[selected_block].index][selected_symbol];
//	auto node = root + info.node_index;
//
//
//	auto cur_node = node;
//	while (cur_node)
//	{
//		ImGui::TableNextRow();
//		ImGui::TableNextColumn();
//		ImGui::PushID(cur_node);
//		ImGui::Selectable(cur_node->name.c_str());
//		if (ImGui::BeginItemTooltip())
//		{
//			ImGui::TextColored(ImVec4(1, 1, 0, 1), cur_node->name.c_str());
//			ImGui::EndTooltip();
//		}
//
//		ImGui::PopID();
//		cur_node = cur_node->parent;
//	}
//}