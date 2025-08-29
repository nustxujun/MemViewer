#include "Component.h"
#include "imgui.h"
#include "Utils.h"
#include <filesystem>
#include <fstream>
#include <regex>

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




void TimelineComp::Show()
{
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();


	ImGui::BeginChild("timeline");
	const bool is_active = ImGui::IsItemActive();   // Held
	const bool is_focus = ImGui::IsItemFocused();

	auto start_pos = ImGui::GetCursorScreenPos(); 
	auto size = ImGui::GetContentRegionAvail();
	auto end_pos = ImVec2(start_pos.x + size.x, start_pos.y + size.y);

	const ImVec2 mouse_pos_in_canvas(io.MousePos.x - start_pos.x, io.MousePos.y - start_pos.y);
	const bool is_in_canvas = io.MousePos.x >= start_pos.x && io.MousePos.x <= end_pos.x && io.MousePos.y <= end_pos.y && io.MousePos.y > start_pos.y;

	// bg
	draw_list->AddRectFilled(start_pos, end_pos, IM_COL32(50, 50, 50, 255));
	draw_list->AddRect(start_pos, end_pos, IM_COL32(255, 255, 255, 255));



	ImGui::EndChild();
}

void TimelineComp::setDatas(int index, TimelineData datas) 
{

}
