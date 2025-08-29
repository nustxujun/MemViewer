//#include "FrequenceView.h"
//#include "TraceParser.h"
//#include "FrameParser.h"
//#include "MemViewer.h"
//#include "Utils.h"
//#include "Concurrency.h"
//
//#include "imgui/imgui.h"
//void FrequenceView::UpdateTimeline()
//{
//	if (timelines.size() == 0)
//		return;
//	std::vector<float> vec = timelines[freq_infos[selected_item].node_index];
//
//	std::transform(vec.begin(), vec.end(), vec.begin(), [&](auto v) {return v * scaling; });
//	SetCustomData2(std::move(vec));
//}
//
//
//void FrequenceView::InitializeImpl()
//{
//	freq_infos.clear();
//	selected_item = 0;
//	auto& data = GetParsedData();
//	auto root = data.root;
//	for (auto& tls : timelines)
//	{
//		tls.second.clear();
//	}
//	timelines.erase(0);
//	std::vector<float> root_list;
//	float max = 0;
//	root_list.resize(data.frame_end - data.frame_begin + 1, 0);
//	for (auto& alloc : data.allocs)
//	{
//		auto stack = root + alloc.node_index;
//		auto& tl = timelines[alloc.node_index];
//		if (tl.size() == 0)
//		{
//			tl.resize(data.frame_end - data.frame_begin + 1, 0);
//		}
//
//		auto frame = std::min(alloc.start, data.frame_end )- data.frame_begin;
//		tl[frame] += 1.0f;
//		root_list[frame] += 1.0f;
//		max = std::max(root_list[frame], max);
//	}
//
//
//	timelines[0] = std::move(root_list);
//
//	auto inv_max = 1.0f / max;
//
//	std::vector<std::vector<float>*> list;
//	for (auto& tls : timelines)
//	{
//		list.push_back(&tls.second);
//	}
//	ParallelTask([&](int idx) {
//		auto& tls = *list[idx];
//		for (auto& tl : tls)
//		{
//			tl *= inv_max;
//		}
//
//		}, timelines.size());
//	//update_timeline();
//	SetCustomData2({});
//}
//
//void FrequenceView::UpdateImpl()
//{
//	Counter c("update freq");
//	selected_item = 0;
//	auto& data = GetParsedData();
//	auto root = data.root;
//
//	std::unordered_map<uint32_t, FreqInfo> alloc_map;
//	alloc_map.reserve(data.allocs.size() + 1);
//
//	auto begin_frame = GetFrameRange().first;
//	auto end_frame = GetFrameRange().second;
//	std::vector<float> root_list;
//	root_list.resize(data.frame_end - data.frame_begin + 1, 0);
//	float max = 0;
//
//
//	auto index = std::lower_bound(data.allocs.begin(), data.allocs.end(), begin_frame, [](auto& a, auto& b) {
//		return a.start < b;
//	});
//
//	for (auto i = index; i < data.allocs.end(); ++i)
//	{
//		auto& alloc = *i;
//
//		if (alloc.start > end_frame)
//			break;
//		auto& info = alloc_map[alloc.node_index];
//
//		info.count += 1;
//		info.node_index = alloc.node_index;
//
//	}
//
//
//	freq_infos.clear();
//	freq_infos.reserve(alloc_map.size());
//	uint32_t total = 0;
//	for (auto& alloc : alloc_map)
//	{
//		total += alloc.second.count;
//		freq_infos.push_back(alloc.second);
//	}
//	freq_infos.push_back({ total, 0,true });
//	std::stable_sort(freq_infos.begin(), freq_infos.end(), [](auto& a, auto& b) {
//		return a.count > b.count;
//	});
//
//	UpdateTimeline();
//}
//
//void FrequenceView::ShowImpl()
//{
//	const ImGuiTableFlags tbl_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
//
//
//	ImGui::BeginChild("symbol", ImVec2(500, 0), ImGuiChildFlags_ResizeX);
//	int step = std::max(1, scaling / 4);
//
//	if (ImGui::InputInt("scaling", &scaling, step, step * 100))
//	{
//		scaling = std::max(1, scaling);
//		UpdateTimeline();
//	}
//
//
//	static char tmp[1024] = {};
//	if (ImGui::InputTextWithHint("##filter", "filter", tmp, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
//	{
//		std::string filter = tmp;
//		const bool is_empty = filter.empty();
//		auto num = std::thread::hardware_concurrency() + 1;
//		auto stride = CalTaskGroupStride(freq_infos.size(), num);
//
//		auto root = GetParsedData().root;
//		
//		ParallelTask([&](int idx){
//			auto begin = idx * stride;
//			auto end = std::min(size_t(idx + 1) * stride, freq_infos.size());
//			for (auto i = begin; i < end; ++i)
//			{
//				auto& info  = freq_infos[i];
//				auto node = root + info.node_index;
//				auto is_matched = [&]() {
//					auto cur = node;
//					while (cur)
//					{
//						if (cur->name.find(filter) != std::string::npos)
//							return true;
//						cur = cur->parent;
//					}
//					return false;
//
//				};
//
//				info.matched = is_empty || is_matched();
//			}
//		
//		}, num);
//	}
//
//	if (ImGui::BeginTable("symbol", 2, tbl_flags))
//	{
//		ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_NoHide);
//		ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 150);
//		ImGui::TableHeadersRow();
//
//
//		if (freq_infos.size() > 0)
//		{
//			MakeOrderedInfos();
//		}
//
//		ImGui::EndTable();
//	}
//	ImGui::EndChild();
//
//	ImGui::SameLine();
//	if (ImGui::BeginTable("stack", 1, tbl_flags))
//	{
//		ImGui::TableSetupColumn("Callstack", ImGuiTableColumnFlags_NoHide);
//		ImGui::TableHeadersRow();
//
//		if (selected_item < freq_infos.size())
//		{
//			MakeStack();
//		}
//
//		ImGui::EndTable();
//	}
//}
//
//void FrequenceView::MakeOrderedInfos() 
//{
//	auto root = GetParsedData().root;
//	uint32_t idx = 0;
//	uint32_t count = 0;
//	for (auto& info : freq_infos)
//	{
//
//		auto node = root + info.node_index;
//
//		if (info.matched)
//		{
//			count++;
//			ImGui::TableNextRow();
//			ImGui::TableNextColumn();
//			ImGui::PushID(node);
//			if (ImGui::Selectable(node->name.c_str(), selected_item == idx, ImGuiSelectableFlags_SpanAllColumns))
//			{
//				if (selected_item != idx)
//				{
//					selected_item = idx;
//
//				}
//				UpdateTimeline();
//			}
//			ImGui::PopID();
//			ImGui::TableNextColumn();
//			ImGui::Text("%d", info.count);
//		}
//
//		idx++;
//
//		if (count > 1000)
//			break;
//	}
//}
//
//void FrequenceView::MakeStack()
//{
//	auto root = GetFrameData();
//
//	auto& info = freq_infos[selected_item];
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