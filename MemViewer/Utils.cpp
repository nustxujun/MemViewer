#include "Utils.h"
#include <Windows.h>
#include <fstream>
#include <filesystem>
#include "TraceParser.h"
#include "FrameParser.h"
#include <sstream>
#include <functional>
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
std::string get_or_create_default_file(int id, const char* path)
{
	auto work_dir = std::filesystem::current_path();
	const auto config_path = work_dir / path;
	if (std::filesystem::exists(config_path))
		return config_path.string();

	auto res = FindResourceA(NULL, MAKEINTRESOURCEA(id), MAKEINTRESOURCEA(10));
	if (!res)
		return {};

	auto mem = LoadResource(NULL, res);
	if (!mem)
		return {};

	auto size = SizeofResource(NULL, res);
	auto data = LockResource(mem);
	if (!data)
		return {};

	std::ofstream f(config_path,std::ios::binary);
	f.write((const char*)data, size);

	return config_path.string();
}


//static void visit_node(Node* node, int depth,const std::function<void(Node*, int)>& visitor)
//{
//	visitor(node, depth);
//	for (auto& c : node->children)
//	{
//		visit_node(c, depth + 1, visitor);
//	}
//}

//Node* make_current_snapshot()
//{
//	return CloneParsedNodes(GetFrameData());
//}
//
//std::string make_snapshot_to_string(SnapshotInfos infos)
//{
//	std::stringstream ret;
//	ret << "snapshot" << std::endl;
//	ret << infos.name << std::endl;
//	ret << infos.count << std::endl;
//
//	for (int i = 1; i < infos.count; ++i)
//	{
//		auto node = infos.root + i;
//		uint32_t parent_offset = node->parent - infos.root;
//		ret << std::format("{}|{}|{}|{}", parent_offset, node->name, node->basic_size, node->basic_count) << std::endl;
//	}
//
//	return ret.str();
//}
//
//SnapshotInfos make_snapshot_from_string(std::string content)
//{
//	SnapshotInfos infos;
//	auto lines = split_string(std::move(content),"\n");
//
//
//	if (lines[0].find("snapshot") == std::string::npos)
//		return {};
//
//	infos.name = lines[1];
//	infos.count = std::atoi(lines[2].c_str());
//	
//	infos.root = new Node[infos.count];
//
//	auto iter = lines.begin() + 3;
//	auto end = lines.end();
//
//	auto node = infos.root + 1;
//	infos.root->name = "root";
//	for (; iter != end;++iter,node++)
//	{
//		auto parts = split_string(*iter, "|");
//		if (parts.empty())
//			continue;
//
//		int parent_offset = std::atoi(parts[0].c_str());
//		int64_t size = std::atoll(parts[2].c_str());
//		int64_t count = std::atoll(parts[3].c_str());
//		node->basic_size = size;
//		node->basic_count = count;
//		node->name = parts[1];
//		
//		auto parent = infos.root + parent_offset;
//		parent->children.push_back(node);
//		node->parent = parent;
//		
//
//	}
//
//	return infos;
//}


static std::string StringConverter(const std::string& str, UINT SrcCodePage, UINT DstCodePage)
{
	int wide_len = MultiByteToWideChar(SrcCodePage, 0, str.c_str(), -1, nullptr, 0);
	std::wstring wide_str(wide_len, 0);
	MultiByteToWideChar(SrcCodePage, 0, str.c_str(), -1, &wide_str[0], wide_len);

	int len = WideCharToMultiByte(DstCodePage, 0, wide_str.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string output(len, 0);
	WideCharToMultiByte(DstCodePage, 0, wide_str.c_str(), -1, &output[0], len, nullptr, nullptr);

	output.pop_back();
	return output;
}

std::string GBKToUTF8(const std::string& gbk_str) {

	return StringConverter(gbk_str, CP_ACP, CP_UTF8);

}
std::string UTF8ToGBK(const std::string& utf_str) {

	return StringConverter(utf_str, CP_UTF8, CP_ACP);

}

bool Spinner(const char* label, float radius, int thickness, const ImU32& color) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);

	ImVec2 pos = window->DC.CursorPos;
	ImVec2 size((radius) * 2, (radius + style.FramePadding.y) * 2);

	const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
	ImGui::ItemSize(bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(bb, id))
		return false;

	// Render
	window->DrawList->PathClear();

	int num_segments = 30;
	int start = abs(ImSin(g.Time * 1.8f) * (num_segments - 5));

	const float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
	const float a_max = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;

	const ImVec2 centre = ImVec2(pos.x + radius, pos.y + radius + style.FramePadding.y);

	for (int i = 0; i < num_segments; i++) {
		const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
		window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a + g.Time * 8) * radius,
			centre.y + ImSin(a + g.Time * 8) * radius));
	}

	window->DrawList->PathStroke(color, false, thickness);
	return true;
}
