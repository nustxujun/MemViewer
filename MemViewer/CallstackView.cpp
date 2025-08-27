#include "CallstackView.h"
#include "Utils.h"
#include "FrameParser.h"
#include "TraceParser.h"
#include "TraceInstance.h"
#include "imgui/imgui.h"


void CallstackView::FilterByString(const char* infilters)
{
	filters = split_string(infilters, "|");
	//const std::string delim = "|";
	//for (const auto& word : std::views::split(std::string(infilters), delim)) {
	//	filters.push_back(std::string(word.begin(), word.end()));
	//}

	if (!case_sensitive)
	{
		for (auto& f : filters)
		{
			f = to_lower(f);
		}
	}

	Update();

}


void CallstackView::CalSize(Node* node, bool ignore_node )
{
	node->size = 0;
	node->count = 0;
	node->is_matched = false;
	if (ignore_node && !filters.empty())
	{

		std::string name;
		if (!case_sensitive)
		{
			name = to_lower(node->name);
		}
		else
		{
			name = node->name;

		}
		for (auto& filter : filters)
		{
			ignore_node = name.find(filter) == std::string::npos;
			if (!ignore_node)
				break;
		}


		node->is_matched = !ignore_node;
	}

	for (auto& child : node->children)
	{
		CalSize(*child, ignore_node);
		node->size += child->size;
		node->count += child->count;
	}

	std::stable_sort(node->children.begin(), node->children.end(), [&](auto n1, auto n2)
		{
			return n1->size > n2->size;
		});

	if (!ignore_node)
	{
		node->size += node->basic_size;
		node->count += node->basic_count;
	}

}


void CallstackView::InitializeImpl()
{
}

void CallstackView::UpdateImpl()
{
	Counter counter("update treeview");

	if (!GetTrace()->getCalltree())
		return;

	calltree = GetTrace()->getCalltree()->clone();

	CalSize(calltree->get(0), !filters.empty());
}

void CallstackView::MakeTreeView(Node* node)
{
	if (node->size == 0)
		return;
	ImGui::TableNextRow();
	ImGui::TableNextColumn();


	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns;

	const bool is_leaf = node->children.size() == 0;

	if (is_leaf)
	{
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	if (node->is_matched)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0.5, 1));

	if (node->parent && (double(node->size) / node->parent->size) >= 0.8 && !node->is_matched)
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	bool open = ImGui::TreeNodeEx(node, flags, std::format("{0:16s} {1}", size_tostring(node->size), node->name).c_str());
	if (node->is_matched)
		ImGui::PopStyleColor();


	ImGui::TableNextColumn();
	ImGui::Text("%d", node->count);

	if (!is_leaf && open)
	{
		for (auto child : node->children)
			MakeTreeView(*child);
		ImGui::TreePop();
	}

}

void CallstackView::ShowImpl()
{

	ImGui::BeginChild("filter", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

	if (ImGui::Checkbox("Aa", &case_sensitive))
	{
		FilterByString(input_buffer);
	}

	ImGui::SameLine();

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

	if (ImGui::InputTextWithHint("##filter", "Input key words to search, use | to split the key words", input_buffer, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		FilterByString(input_buffer);
	}



	ImGui::EndChild();

	ImGui::BeginChild("treeview", ImVec2(0, 0));

	static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

	if (ImGui::BeginTable("treeviewtable", 2, flags))
	{
		ImGui::TableSetupColumn("Tree", ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 100);
		ImGui::TableHeadersRow();

		if (calltree)
			MakeTreeView(calltree->get(0));
		ImGui::EndTable();
	}



	ImGui::EndChild();
}