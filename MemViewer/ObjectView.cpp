#include "ObjectView.h"
#include "TraceParser.h"
#include "FrameParser.h"
#include "MemViewer.h"
#include "TraceInstance.h"
#include "Utils.h"
#include "imgui/imgui.h"
#include "resource.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <queue>


static auto less = [](auto& a, auto& b) {
	return a < b;
};

static auto greater = [](auto& a, auto& b) {
	return a > b;
};

auto cmp = [&](auto& a, auto& b, bool l) {
	if (l)
		return less(a, b);
	else
		return greater(a, b);
};

auto align_pow2 = [](auto in)
{
	int i = 1;
	while(in > (1 << i))
		i++;

	return 1 << i;
};

static const ImGuiTableFlags tbl_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Sortable;


void ObjectView::InitializeImpl()
{
	//type_filters.clear();
	{
		//auto f = std::fstream(get_or_create_default_file(IDR_OBJECT_CONFIG_INI, "object_config.ini"), std::ios::in);
		//std::string content;

		//std::string type_name; 
		//while (!f.eof() && f.good())
		//{
		//	char temp[1024];
		//	f.getline(temp, 1024);
		//	std::regex pattern("(.+)=(.+)");
		//	std::smatch results;
		//	std::string content = temp;
		//	bool is_sub = temp[0] == '+';


		//	if (std::regex_search(content, results, pattern) )
		//	{
		//		assert(is_sub);
		//		type_filters[type_name][results[1].str().substr(1)] = split_string(results[2], "|");
		//	}
		//	else
		//	{
		//		type_name = temp;
		//	}
		//}
	}
}

void ObjectView::UpdateImpl()
{
	selected = 0;
	root = std::make_shared<ObjectNode>();
	root->name = "root";
	auto& objs = GetTrace()->getObjects();


	std::string filter_str = filter;

	auto create_type_tree = [&](auto node, auto& obj, int depth, auto& add)->void
	{
		
		if (depth >= obj.class_chain->name_chain.size())
		{
			node->objs.push_back(&obj);
			node->total_add++;
			return;
		}
		auto& cur_type = obj.class_chain->name_chain[depth];
		for (auto& c : node->children)
		{
			if (cur_type == c->name)
			{
				add(c, obj, depth + 1, add);
		
				node->total_add++;
				return;
			}
		}

		auto new_child = std::make_shared<ObjectNode>();
		new_child->name = cur_type;
		node->children.push_back(new_child);

		add(new_child, obj, depth + 1, add);
		node->total_add++;
	};

	std::map<int, ObjectNode::Ptr> node_map;
	auto make_tree_from_calltree= [&](){
		auto calltree = GetTrace()->getCalltree();

		auto add_stack_node = [&](auto node, auto tree_node, auto& add)->void
		{
			for(auto& c : tree_node->children)
			{
				auto new_child = std::make_shared<ObjectNode>();

				new_child->name = c->name;
				node_map.emplace(c.index, new_child);
				node->children.push_back(new_child);

				add(new_child, *c,add);
			}
		};

		add_stack_node(root, calltree->root, add_stack_node);
	};

	if (selected_show_type == 1)
		make_tree_from_calltree();

	auto create_stack_tree = [&]( auto& obj)
	{
		std::vector<NodeRef> stack_chain;

		auto node = node_map[obj.node.index];
		node->objs.push_back(&obj);
	};

	for (auto& obj : objs)
	{
		EXIT_IF_STOPED()
		if (selected_show_type == 0)
		create_type_tree(root, obj, 0, create_type_tree);
		else if (selected_show_type == 1)
			create_stack_tree( obj);
	}


	auto reorder = [&](auto node, auto& ro, int init_matching)->bool{


		node->total_add = 0;
		node->total_delete = 0;

		node->matching = init_matching;
		if (filter_str.empty())
			node->matching |= 2;
		else if (node->name.find(filter_str) != std::string::npos)
		{
			node->matching |= 1;
		}

		auto matching = node->matching > 0;
		{
			auto iter = node->children.begin();
			for (; iter != node->children.end(); )
			{
				bool skip = !(matching );
				if (ro(*iter, ro, matching ? 4 : 0))
				{
					skip = false;
					node->matching |= 2;
				}
				if (((*iter)->total_add == 0 && (*iter)->total_delete == 0 ) || skip)
				{
					iter = node->children.erase(iter);
				}
				else
				{
					node->total_add += (*iter)->total_add;
					node->total_delete += (*iter)->total_delete;
					iter++;
				}
			}
		}

		if (!matching)
		{
			auto iter = node->objs.begin();
			for (; iter != node->objs.end();)
			{
				auto obj = *iter;
				if (init_matching > 0 || obj->name.find(filter_str) != std::string::npos)
				{
					node->matching |= 2;
					iter++;
				}
				else
				{
					iter = node->objs.erase(iter);
				}
			}
		
		}

		std::stable_sort(node->objs.begin(), node->objs.end(), [](auto& a, auto& b) {
			if (a->state != b->state)
				return a->state > b->state;
			return a->name < b->name;
		});
		std::stable_sort(node->children.begin(), node->children.end(), [](auto& a, auto& b) {
			if ( a->total_add != b->total_add)
				return a->total_add > b->total_add;
			return a->total_delete < b->total_delete;
		});

		for (auto& obj : node->objs)
		{
			if (obj->state >= 0)
				node->total_add++;
			else
				node->total_delete++;
		}
		//node->total_count += (uint32_t)node->objs.size();

		return node->matching > 0;
	};

	reorder(root, reorder, 0);

}


void ObjectView::ShowImpl()
{

	static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
	if (ImGui::BeginTable("objs",2, flags))
	{

		ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);


	auto space = ImGui::GetContentRegionAvail();
	ImGui::BeginChild("class", space, 0 , ImGuiWindowFlags_HorizontalScrollbar);


	ImGui::SetNextItemWidth(250);
	if (ImGui::InputTextWithHint("##class_filter", "", filter, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		Update();
	}

	ImGui::SetNextItemWidth(250);
	const char* types[] = {"show by class", "show by stack"};
	if (ImGui::Combo("show type", &selected_show_type, types, 2))
	{
		Update();
	}

	std::string filter_str = filter;

	auto draw_node = [&](auto node, bool is_parent_standalone,auto& draw)->void{
		if (node->matching == 0)
			return ;

		if (is_parent_standalone)
			ImGui::SetNextItemOpen(true, ImGuiCond_Once);

		if (node->objs.size() == 0 && node->children.size() == 1)
		{
			is_parent_standalone = true;
		}
		else
		{
			is_parent_standalone = false;
		}

		if (node->matching & 1)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 1, 1, 1));
		else if (node->matching & 4)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8, 0.8, 0.8, 1));

		}

		if (ImGui::TreeNodeEx(std::format("add:{:<6} remove:{:<6} {}", node->total_add, node->total_delete, node->name).c_str(), ImGuiTreeNodeFlags_SpanAllColumns))
		{
			if (node->objs.size() > 0)
			{

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0.5, 1));

				if (ImGui::TreeNodeEx(std::format("contains {} objects", node->objs.size()).c_str(), ImGuiTreeNodeFlags_SpanAllColumns))
				{

					for (auto& obj : node->objs)
					{
						if (obj->state < 0)
						{
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.25, 0.25, 1));
						}
						else if (!filter_str.empty() && obj->name.find(filter_str) != std::string::npos)
						{
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 1, 1, 1));
						}
						else if (node->matching & 5)
						{
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
						}
						else
						{
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8, 0.8, 0.8, 1));
						}

						if (ImGui::Selectable(std::format("  {}", obj->name).c_str(), selected == obj))
						{
							selected = obj;
						}
						ImGui::PopStyleColor();
					}
					ImGui::TreePop();
				}

				ImGui::PopStyleColor();
			}

			for (auto& c : node->children)
			{
				draw(c, is_parent_standalone,draw);
			}




			ImGui::TreePop();
		}

		ImGui::PopStyleColor();

	};

	if (root)
	draw_node(root,false, draw_node);

	ImGui::EndChild();
	ImGui::TableSetColumnIndex(1);


	ImGui::BeginChild("info");
	if (selected_show_type == 0 && selected)
	{
		auto node = selected->node;
		if (ImGui::BeginTable("callstack", 1))
		{
			while(node)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::Text(node->name.c_str());

				node = node->parent;
			}
			ImGui::EndTable();
		}
	}
	else if (selected)
	{
		if (ImGui::BeginTable("nodetype", 1))
		{
			for (auto& n : selected->class_chain->name_chain)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::Text(n.c_str());

			}
			ImGui::EndTable();
		}
	}
	ImGui::EndChild();

	ImGui::EndTable();
	}
	//ShowUIAtlasViewer();
	//ShowSimulation();

}

