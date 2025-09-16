#include "Component.h"
#include "imgui.h"
#include "Utils.h"
#include <filesystem>
#include <fstream>
#include <regex>

#pragma optimize("", off)


bool FilterComp::Show(int flag) 
{
	ImGui::PushID(this);
	auto ret = ImGui::InputText("filter",str, sizeof(str),flag);
	ImGui::PopID();
	return ret;
}
static const ImGuiTableFlags tbl_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate;

void TableComp::Init(std::vector<Table> tbls)
{

	tables = std::move(tbls);
}

void TableComp::Show()
{
	int unique_id = 0;
	auto get_id = [&]()
	{
		return unique_id++;
	};

	int bskip = true;
	bool is_dirty = true;
	for (auto& tbl : tables)
	{
		if (bskip)
		{
			bskip = false;
		}
		else
		{
			ImGui::SameLine();
		}
		ImGui::BeginChild(tbl.name.c_str(), ImVec2(tbl.width, 0), tbl.flags);

		if (!!tbl.header)
		{
			tbl.header();
		}

		if (ImGui::BeginTable(tbl.name.c_str(), tbl.col_names.size(), tbl_flags))
		{	
			// header
			auto count = tbl.col_names.size();
			for (int i = 0; i < count; ++i)
			{
				ImGui::TableSetupColumn(tbl.col_names[i].c_str(),tbl.col_flags[i],tbl.col_widths[i]);
			}
			ImGui::TableHeadersRow();

			// sort
			if (auto item = ImGui::TableGetSortSpecs())
			{
				if (item->Specs && (item->SpecsDirty || is_dirty))
				{
					tbl.on_sort(item->Specs->ColumnIndex, item->Specs->SortDirection == ImGuiSortDirection_Ascending);
					item->SpecsDirty = false;
				}
			}

			// items
			int index = 0;
			std::vector<std::string> item;


			do 
			{
				item.clear();
				auto is_end = !tbl.item_callback(index++, item);
				if (is_end)
					break;
				if (item.empty())
					continue;

				ImGui::TableNextRow();
				bool bfirst = true;
				for (auto& str : item)
				{
					ImGui::TableNextColumn();
					if (bfirst)
					{
						ImGui::PushID(get_id());
						if (ImGui::Selectable(str.c_str(), tbl.selected == index - 1, ImGuiSelectableFlags_SpanAllColumns))
						{
							tbl.selected = index - 1;
							tbl.selected_name = str;
							tbl.on_select(tbl.selected);
							is_dirty = true;
						}
						ImGui::PopID();
						bfirst = false;
					}
					else
					{
						ImGui::Text(str.c_str());
					}
				}	
			} while (true);	

			ImGui::EndTable();
		}
		ImGui::EndChild();
	}
}

std::vector<CategoryComp::Category>& CategoryComp::Init(const std::string& file_path)
{
	config_file = file_path;
	groups.clear();
	table.Init({});
	LoadFile();
	selected_category = 0;
	selected_subcategory = 0;
	selected_item = 0;
	return groups;
}

std::vector<CategoryComp::Category>& CategoryComp::Init(const std::vector<std::string>& cate)
{
	groups.clear();
	for (auto& c : cate)
	{
		groups.push_back(c);
	}
	table.Init({});
	selected_category = 0;
	selected_subcategory = 0;
	selected_item = 0;
	return groups;
}

void CategoryComp::LoadFile()
{
	groups.clear();
	auto work_dir = std::filesystem::current_path();
	const auto config_path = work_dir / config_file;
	if (std::filesystem::exists(config_path))
	{
		auto size = std::filesystem::file_size(config_path);
		auto f = std::fstream(config_path, std::ios::in);
		std::string content;
		char temp[1024 * 4] = {};
		while (!f.eof() && f.good())
		{

			f.getline(temp, 1024 * 4);
			std::regex pattern("([^=]+)=?([^=]*)");
			std::smatch results;
			std::string content = temp;
			const bool is_sub = temp[0] == '+';

			if (std::regex_search(content, results, pattern))
			{
				if (is_sub)
				{
					auto& grp = groups[groups.size() - 1];
					grp.subs.emplace_back(results[1].str().substr(1));
					auto& sub = *grp.subs.rbegin();
					sub.keys = split_string(results[2], "|");
				}
				else
				{
					groups.emplace_back(results[1]);
					auto& grp = *groups.rbegin();
					grp.keys = split_string(results[2], "|");
				}
			}

		}
	}

	for (auto& grp : groups)
	{
		grp.subs.push_back(SubCategory("untagged"));
	}
}

void CategoryComp::InitTable(std::function<void(void*)>&& on_selected_item, std::function<void(const std::string&, const std::string&)>&& on_show, std::function<bool(bool, void*, const std::string& key)>&& filter)
{

	auto cmp = [](auto& a, auto& b, bool l) {
		if (l)
			return std::less()(a, b);
		else
			return std::greater()(a, b);
	};

	auto cmp2 = [cmp](int col, int dir, auto& a, auto& b) {
		switch (col)
		{
		case 0: return cmp(a.name, b.name, dir);
		case 1: return cmp(a.size, b.size, dir);
		case 2: return cmp(a.count, b.count, dir);
		}

		return false;
	};

	TableComp::Table tbl;
	tbl.name = "category";
	tbl.flags = ImGuiChildFlags_AutoResizeX;
	tbl.width = 0;
	tbl.col_names = { "name", "size", "count" };
	tbl.col_flags = { ImGuiTableColumnFlags_NoHide , ImGuiTableColumnFlags_WidthFixed ,ImGuiTableColumnFlags_WidthFixed };
	tbl.col_widths = { 0,150,100 };
	tbl.item_callback = [&](int idx, std::vector<std::string>& list)->bool
	{
		if (idx >= groups.size())
			return false;

		auto& grp = groups[idx];
		list = { grp.name, size_tostring(grp.size), std::to_string(grp.count) };
		return true;
	};
	tbl.on_select = [&, on_selected_item](int selected) {
		selected_category = selected;
		selected_subcategory = 0;
		selected_item = 0;
		on_selected_item(0);
	};
	tbl.on_sort = [&, cmp2](int col, int dir){
		std::stable_sort(groups.begin(), groups.end(), [&](auto& a, auto& b){
			return cmp2(col, dir, a, b);
		});
	};

	tbl.on_sort(1,0);

	auto sub = tbl;
	sub.name = "subcategory";

	sub.item_callback = [&](int idx, std::vector<std::string>& list) {
		auto& grps = groups[selected_category].subs;
		if (idx >= grps.size())
			return false;

		auto& grp = grps[idx];
		list = { grp.name, size_tostring(grp.size), std::to_string(grp.count) };
		return true;
	};

	sub.on_select = [&, on_selected_item](int selected) {
		selected_subcategory = selected;
		selected_item = 0;
		on_selected_item(0);
	};

	sub.on_sort = [&, cmp2](int col, int dir) {
		auto& grps = groups[selected_category].subs;

		std::stable_sort(grps.begin(), grps.end(), [&](auto& a, auto& b) {
			return cmp2(col, dir, a, b);
		});
	};

	auto item = tbl;
	item.name = "item";
	item.item_callback = [&,filter](int idx, std::vector<std::string>& list){
		if (selected_subcategory >= groups[selected_category].subs.size())
			return false;
		auto& sub = groups[selected_category].subs[selected_subcategory];
		if (idx >= sub.datas.size())
			return false;

		auto& item = sub.datas[idx];

		if (!filter(filter_comp.filter(item.name), item.userdata, filter_comp.GetString()))
			return true;

		show_size += item.size;
		show_count += item.count;
		list = {item.name, size_tostring(item.size), std::to_string(item.count)};
		return true;
	};
	item.on_select = [&, on_selected_item](int selected) {
		auto& grps = groups[selected_category].subs;
		auto& subs = grps[selected_subcategory];

		selected_item = selected;
		auto& item = subs.datas[selected_item];

		on_selected_item(item.userdata);
	};

	item.on_sort = [&, cmp2](int col, int dir) {
		auto& subs = groups[selected_category].subs;
		if (subs.size() <= selected_subcategory)
			return;

		auto& datas = subs[selected_subcategory].datas;

		std::stable_sort(datas.begin(), datas.end(), [&](auto& a, auto& b) {
			return cmp2(col, dir, a, b);
		});
	};

	item.header = [&, on_show](){

		auto& subs = groups[selected_category].subs;
		if (subs.size() <= selected_subcategory)
			return;

		auto& sub = subs[selected_subcategory];

		on_show(groups[selected_category].name, sub.name);
		filter_comp.Show();
		ImGui::Text("size: %s, count: %d", size_tostring(show_size).c_str(), show_count);
		show_size = 0;
		show_count = 0;
	};


	table.Init({ tbl, sub, item });

}

void CategoryComp::Update( std::function<void(void*)>&& on_selected_item, std::function<void(const std::string&, const std::string&)>&& on_show, std::function<bool(bool,void*, const std::string& key)>&& filter)
{
	InitTable(std::move(on_selected_item),std::move(on_show), std::move(filter));
}

void CategoryComp::Show()
{
	if (groups.empty())
		return;

	table.Show();
}

void TableList::Init(const std::vector<TableDescriptor>& tbls)
{ 
	tables = tbls;
}


void TableList::Show()
{
	int unique_id = 0;
	auto get_id = [&]()
		{
			return unique_id++;
		};

	int bskip = true;
	int tblcount = 0;


	ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody;


	auto count = tables.size();
	if (count > 0 )
	{
		if (ImGui::BeginTable("table", count, flags))
	{
		ImGui::TableNextRow();
		for (int i = 0; i < count; ++i)
		{
			auto& tbl = tables[i];
			ImGui::TableSetColumnIndex(i);
			if (is_need_clear)
			{
				tbl.selected = 0;
				//tbl.on_selected(tbl.selected);
				tbl.Refresh();

			}
			if (bskip)
			{
				bskip = false;
			}
			else
			{
				ImGui::SameLine();
			}



			ImGui::BeginChild((ImGuiID)&tbl);
			if (tbl.pre_show)
			{
				tbl.pre_show(tbl);
			}
			if (ImGui::BeginTable(std::to_string(tblcount++).c_str(), tbl.header_caches.size(), tbl_flags))
			{
				// header
				auto count = tbl.header_caches.size();
				for (int i = 0; i < count; ++i)
				{
					auto& header = tbl.header_caches[i];
					ImGui::TableSetupColumn(header.name.c_str(), header.flag, header.width);
				}
				ImGui::TableHeadersRow();

				// sort
				if (auto item = ImGui::TableGetSortSpecs())
				{
					if ( item->Specs  && (item->SpecsDirty || is_need_clear))
					{
						tbl.on_sort(item->Specs->ColumnIndex, item->Specs->SortDirection == ImGuiSortDirection_Ascending);
						item->SpecsDirty = false;
						tbl.Refresh();
					}
				}

				// items
				for (int i = 0; i < tbl.item_caches.size(); ++i)
				{
					auto index = tbl.item_caches[i].first;
					auto& item = tbl.item_caches[i].second;
					ImGui::TableNextRow();
					bool bfirst = true;
					for (auto& str : item)
					{
						ImGui::TableNextColumn();
						if (bfirst)
						{
							ImGui::PushID(get_id());
							if (ImGui::Selectable(str.c_str(), tbl.selected == index, ImGuiSelectableFlags_SpanAllColumns))
							{
								tbl.selected = index;
								tbl.on_selected(tbl.selected);
								is_need_clear = true;
							}
							ImGui::PopID();
							bfirst = false;
						}
						else
						{
							ImGui::Text(str.c_str());
						}
					}
				}

				ImGui::EndTable();
			}
			ImGui::EndChild();
		}

		ImGui::EndTable();
	}
	}
	is_need_clear = false;
}

std::vector<CategoryParser::Category> CategoryParser::operator()(const std::string& file_path)
{
	std::vector<CategoryParser::Category> groups;
	auto work_dir = std::filesystem::current_path();
	const auto config_path = work_dir / file_path;
	if (std::filesystem::exists(config_path))
	{
		auto size = std::filesystem::file_size(config_path);
		auto f = std::fstream(config_path, std::ios::in);
		std::string content;
		char temp[1024 * 4] = {};
		while (!f.eof() && f.good())
		{

			f.getline(temp, 1024 * 4);
			std::regex pattern("([^=]+)=?([^=]*)");
			std::smatch results;
			std::string content = temp;
			const bool is_sub = temp[0] == '+';

			if (std::regex_search(content, results, pattern))
			{
				if (is_sub)
				{
					auto& grp = groups[groups.size() - 1];
					grp.subs.emplace_back(results[1].str().substr(1));
					auto& sub = *grp.subs.rbegin();
					sub.keys = split_string(results[2], "|");
				}
				else
				{
					groups.emplace_back(results[1]);
					auto& grp = *groups.rbegin();
					grp.keys = split_string(results[2], "|");
				}
			}

		}
	}

	for (auto& grp : groups)
	{
		grp.subs.push_back(SubCategory("untagged"));
	}

	return groups;
}


static std::function<void()> ShowWindow;
static std::string title;
static bool trigger_modal = false;
void ModalWindow::OpenModalWindow(std::string val, std::function<void()>&& win)
{
	title = std::move(val);
	ShowWindow = std::move(win);
	trigger_modal = true;
}
void ModalWindow::ProcessModalWindow()
{
	if (trigger_modal)
	{
		ImGui::OpenPopup(title.c_str());
		trigger_modal = false;
	}
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(title.c_str(),0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		ShowWindow();

		ImGui::EndPopup();
	}
}




void TimelineComp::Show(float width, float height, OnEvnet&& evn)
{
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();


	ImGui::BeginChild("timeline", { width , height});

	auto start_pos = ImGui::GetCursorScreenPos();
	start_pos.x += 1;
	start_pos.y += 1;
	auto size = ImGui::GetContentRegionAvail();
	auto end_pos = ImVec2(start_pos.x + size.x, start_pos.y + size.y);

	ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool is_active = ImGui::IsItemActive();   // Held
	const bool is_focus = ImGui::IsItemFocused();



	const ImVec2 mouse_pos_in_canvas(io.MousePos.x - start_pos.x, io.MousePos.y - start_pos.y);
	const bool is_in_canvas = io.MousePos.x >= start_pos.x && io.MousePos.x <= end_pos.x && io.MousePos.y <= end_pos.y && io.MousePos.y > start_pos.y;

	// clip
	draw_list->PushClipRect({ start_pos.x + 1, start_pos.y + 1}, {end_pos.x - 1, end_pos.y - 1}, true);

	// bg
	draw_list->AddRectFilled(start_pos, end_pos, IM_COL32(50, 50, 50, 255));
	draw_list->AddRect(start_pos, end_pos, IM_COL32(255, 255, 255, 255));

	// scrolling 
	const float mouse_threshold_for_pan = true ? -1.0f : 0.0f;
	if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan))
	{
		scrolling.x += io.MouseDelta.x;
		scrolling.y += io.MouseDelta.y;
	}

	if (is_in_canvas && io.MouseWheel != 0)
	{
		auto len = (mouse_pos_in_canvas.x - scrolling.x) * freq;

		int diff = std::max<int>(1.0f, freq * 0.1f);
		if (io.MouseWheel > 0)
		{
			freq += diff;
		}
		else
		{
			freq = std::max(1, freq - diff);
		}

		scrolling.x = mouse_pos_in_canvas.x - len / freq;
	}

	//if (/*is_focus &&*/ io.MousePos.x >= start_pos.x && io.MousePos.x <= end_pos.x && is_scrolling)
	//{
	//	if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
	//	{
	//		auto scroll_width = scrollbar_end - scrollbar_start;
	//		int frame_pos = (io.MousePos.x - start_pos.x - scroll_width * 0.5f) * frames.size() / canvas_sz.x;
	//		scrolling.x = (-frame_pos * frame_width);
	//	}
	//	else
	//	{
	//		is_scrolling = false;
	//	}
	//}

	// constants
	const float grid_width = 8;
	const float frame_width = grid_width / freq;
	const float local_begin_x = scrolling.x + start_pos.x;
	const float begin_x = local_begin_x - fmodf(local_begin_x, frame_width);
	const auto frame_scaling = (size.y * 0.8f) / MaxHight;
	const auto std_step = frame_scaling * 512;
	const auto real_frame_width = std::max(1.0f, frame_width <= 3 ? frame_width : frame_width - 1);


	uint32_t overlay_frame = std::floor((io.MousePos.x - begin_x) / frame_width);

	int text_frame = -1;

	std::vector<TimelineData*> ordered_datas;
	for (auto& data : datas)
	{
		if (data.type != TimelineData::None && data.visible)
			ordered_datas.push_back(&data);
	}

	std::stable_sort(ordered_datas.begin(), ordered_datas.end(), [](auto& a, auto& b) {
		return a->order < b->order;
	});

	//data 
	{
		std::vector<float> stacking;
		stacking.resize(Count,0);
		for (auto& data : ordered_datas)
		{
			std::vector<ImVec2> line_points;
			for (auto frame = 0; frame < Count; ++frame)
			{
				auto x = begin_x + frame * frame_width;
				if (x < start_pos.x)
					continue;

				if (x > end_pos.x)
					break;


				auto rect_min_y = end_pos.y - data->datas[frame] * frame_scaling;


				if (data->type == TimelineData::Bar)
				{
					auto p_x = x;
					auto p_y = rect_min_y;
					auto p_z = x + real_frame_width;
					auto p_w = end_pos.y;

					if (data->stacking)
					{
						p_y = p_y - stacking[frame];
						p_w = p_w - stacking[frame];

						stacking[frame] += p_w - p_y;
					}
					draw_list->AddRectFilled({p_x, p_y}, {p_z, p_w}, data->color);

					//line_points.push_back({ x ,rect_min_y });
				}
				else if (data->type == TimelineData::Line)
				{

					line_points.push_back({ x + real_frame_width * 0.5f,rect_min_y });
				}


			}

			if (line_points.size() > 0)
			{
				if (data->type == TimelineData::Bar)
				{
				}

				else
					draw_list->AddPolyline(line_points.data(), line_points.size(), data->color, 0,2.0f);

				//for (int frame = 0; frame < ((int)line_points.size() - 1); ++frame)
				//{
				//	draw_list->AddLine(line_points[frame], line_points[frame + 1], data->color, 2.0f);
				//}
			}
		}
	}


	//drag
	if (is_in_canvas && !is_scrolling)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !is_dragging && is_focus)
		{
			is_dragging = true;
			dragging_start = overlay_frame;
		}
		if (is_dragging)
		{
			dragging_end = overlay_frame;
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				is_dragging = false;
				if (dragging_end == dragging_start)
					dragging_start = 0;

				if (evn)
				evn(dragging_start , dragging_end, true );


			}
		}
	}

	// selected
	{

		auto x0 = begin_x + dragging_start * frame_width;
		auto x1 = begin_x + dragging_end * frame_width;

		auto min_x = std::min(x0, x1);
		auto max_x = std::max(x0, x1);

		draw_list->AddRectFilled(ImVec2(min_x, start_pos.y), ImVec2(max_x + frame_width, end_pos.y), IM_COL32(255, 0, 0, 40));
	}

	//for (float y = end_pos.y, i = 1; y >= (start_pos.y - 20); y -= std_step, i++)
	//{
	//	draw_list->AddLine(ImVec2(start_pos.x, y - std_step), ImVec2(end_pos.x, y - std_step), IM_COL32(200, 200, 200, 100));

	//	auto num = std::format("{0} MB", i * 512);
	//	auto str_len = ImGui::CalcTextSize(num.c_str(), num.c_str() + num.length());
	//	draw_list->AddText(ImVec2(end_pos.x - str_len.x - 5, y - std_step - str_len.y), IM_COL32(200, 200, 200, 200), num.c_str());
	//}



	// graduation
	{
		int index = 0;
		for (auto x = begin_x; x < end_pos.x; x += grid_width, index++)
		{
			if (x < 0)
				continue;

			const auto draw_x = x;

			if (index % 5 == 0)
			{
				if (index % 10 == 0)
				{
					draw_list->AddLine(ImVec2(draw_x, start_pos.y), ImVec2(draw_x, start_pos.y + 10), IM_COL32(255, 255, 255, 255), 2.0f);
					auto num = std::format("{0}", index * freq);
					auto str_len = ImGui::CalcTextSize(num.c_str(), num.c_str() + num.length());
					draw_list->AddText(ImVec2(draw_x - str_len.x * 0.5f, start_pos.y + 12), IM_COL32_WHITE, num.c_str());
				}
				else
					draw_list->AddLine(ImVec2(draw_x, start_pos.y), ImVec2(draw_x, start_pos.y + 7), IM_COL32(255, 255, 255, 100), 1.0f);

			}
			else
			{
				draw_list->AddLine(ImVec2(draw_x, start_pos.y), ImVec2(draw_x, start_pos.y + 4), IM_COL32(255, 255, 255, 100), 1.0f);
			}

		}


		index = 0;
		float grid_height = 0;
		int grid_size = 5;
		do
		{
			grid_size *= 2;

			grid_height = grid_size * frame_scaling;
		}while(grid_height < 8.0f);

		for (auto y = end_pos.y ; y >= 200; y -= grid_height, index++)
		{
			if (y < 0)
				continue;

			const auto draw_y = y;
			const auto draw_x = end_pos.x ;

			if (index % 5 == 0)
			{
				if (index % 10 == 0)
				{
					draw_list->AddLine(ImVec2(draw_x - 10, draw_y), ImVec2(draw_x, draw_y), IM_COL32(255, 255, 255, 255), 2.0f);
					auto num = std::format("{0}", grid_size * index);
					auto str_len = ImGui::CalcTextSize(num.c_str(), num.c_str() + num.length());
					draw_list->AddText(ImVec2(draw_x - 12 - str_len.x , draw_y - 15), IM_COL32_WHITE, num.c_str());
				}
				else
					draw_list->AddLine(ImVec2(draw_x - 7, draw_y), ImVec2(draw_x, draw_y), IM_COL32(255, 255, 255, 255), 1.0f);

			}
			else
			{
				draw_list->AddLine(ImVec2(draw_x - 4, draw_y), ImVec2(draw_x, draw_y), IM_COL32(255, 255, 255, 255), 1.0f);
			}

		}

	}

	// overlay
	{


		draw_list->AddRectFilled(ImVec2(begin_x + overlay_frame * frame_width, start_pos.y), ImVec2(begin_x + overlay_frame * frame_width + real_frame_width, end_pos.y), IM_COL32(255, 255, 255, 100));
		draw_list->AddLine(ImVec2(start_pos.x, io.MousePos.y), ImVec2(end_pos.x, io.MousePos.y), IM_COL32(255, 255, 255, 200));
	}


	// text
	if (overlay_frame >= 0 && overlay_frame < Count)
	{
		struct TextContent
		{
			std::string text;
			float size;
			int color;
		};


		std::vector<TextContent> contents;
		float max_width = 0;
		for (auto& data : datas)
		{
			if (data.visible == false || data.type == TimelineData::None)
				continue;
			auto content = std::format("{:32s}: {:.2f}", data.name, data.datas[overlay_frame]);
			contents.push_back({ content,data.datas[overlay_frame], data.color });
			auto size = ImGui::CalcTextSize(content.c_str());
			max_width = std::max(max_width, size.x);
		}

		std::stable_sort(contents.begin(), contents.end(), [](auto& a, auto& b) {return a.size > b.size; });


		const float height = 20;


		auto tx = io.MousePos.x + 20;
		auto ty = io.MousePos.y + 20;

		int extra_lines = 2;
		draw_list->AddRectFilled({ tx, ty }, { tx + max_width, ty + height * (contents.size() + extra_lines) }, IM_COL32_BLACK, 0.5f);

		draw_list->AddText({ tx, ty }, IM_COL32_WHITE, std::to_string(overlay_frame).c_str());
		draw_list->AddText({ tx, ty + height }, IM_COL32_WHITE, std::format("{:.2f}" ,(end_pos.y - io.MousePos.y) / frame_scaling).c_str());

		for (int i = 0; i < contents.size(); ++i)
		{
			auto& c = contents[i];
			draw_list->AddText({ tx, ty + height * (i + extra_lines)}, c.color, c.text.c_str());
		}
	}


	ImGui::EndChild();
}

void TimelineComp::setDatas(int index, TimelineData data) 
{
	Count = std::max<int>(Count, data.datas.size());

	
	for (auto& d : data.datas)
	{
		MaxHight = std::max<float>(MaxHight, d);
	}

	data.datas.resize(Count);
	//datas.push_back(std::move(data));
	if (datas.size() <= index)
	{
		datas.resize(index + 1);
	}

	datas[index] = std::move(data);
}
