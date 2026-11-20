#include "Utils.h"
#include <Windows.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <functional>
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <mutex>

static void memviewer_print_log(const std::string& content)
{
	static std::mutex Mutex;
	std::lock_guard<std::mutex> lock(Mutex);
	std::cout << content << std::endl;
}
static struct _InitLog { _InitLog() { g_print_log_func = memviewer_print_log; } } _init_log;


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
