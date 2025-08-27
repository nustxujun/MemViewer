#include "Concurrency.h"
#include "Utils.h"
#include "imgui.h"
#include "imgui/imgui_internal.h"
#include <mutex>

std::vector<Task> task_list;
std::mutex mutex;


static bool Spinner(const char* label, float radius, int thickness, const ImU32& color) {
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

void AddTask(Task&& task)
{
	std::lock_guard lock(mutex);
	task_list.push_back(std::move(task));
}


void ProcessTasks()
{
	std::vector<Task> task_ready_list;
	size_t count_task = 0;
	std::vector<std::string> waiting_list;
	do
	{
		std::lock_guard lock(mutex);
		count_task = task_list.size();
		if (count_task == 0)
			break;

		bool no_task_ready = true;
		for (auto i = task_list.begin(); i != task_list.end();)
		{
			auto status = i->wait_for(std::chrono::nanoseconds(0));
			if (status == std::future_status::ready)
			{
				task_ready_list.push_back(std::move(*i));
				i = task_list.erase(i);
			}
			else
			{
			
				++i;
			}
		}
	}while(false);

	for (auto& task : task_ready_list)
	{
		task.get()();
	}

}

void ShowWaitingTaskWindow()
{
	//std::vector<Task> task_ready_list;
	//size_t count_task = 0;
	//std::vector<std::string> waiting_list;
	//do
	//{
	//	std::lock_guard lock(mutex);
	//	count_task = task_list.size();
	//	if (count_task == 0)
	//		break;

	//	bool no_task_ready = true;
	//	auto end = task_list.end();
	//	for (auto i = task_list.begin(); i != end;)
	//	{
	//		auto status = i->task.wait_for(std::chrono::nanoseconds(0));
	//		if (status == std::future_status::ready)
	//		{
	//			task_ready_list.push_back(std::move(i->task));
	//			i = task_list.erase(i);
	//		}
	//		else
	//		{
	//			if (i->need_waitting)
	//			{
	//				waiting_list.push_back(i->name);
	//			}
	//			++i;
	//		}
	//	}
	//}while(false);

	//for (auto& task : task_ready_list)
	//{
	//	task.get()();
	//}


	//if (!waiting_list.empty())
	//{
	//	ImGui::OpenPopup("waiting");

	//}

	//ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	//ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	//if (ImGui::BeginPopupModal("waiting",0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	//{
	//	Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
	//	ImGui::SameLine();
	//	ImGui::Text("Waiting for :");
	//	for (auto& n : waiting_list)
	//	{
	//		ImGui::Text(n.c_str());
	//	}

	//	if (waiting_list.empty())
	//		ImGui::CloseCurrentPopup();

	//	ImGui::EndPopup();
	//}
}


void ParallelTask(std::function<void(int)>&& task, uint32_t count)
{
	std::vector<std::future<void>> threads;
	auto num_threads = std::thread::hardware_concurrency() + 1;
	
	uint32_t stride = count / num_threads + (count % num_threads ? 1: 0);


	threads.reserve(num_threads);
	for (uint32_t i = 0; i < num_threads; ++i)
	{
		threads.push_back(std::async([=, &task ](){
			auto start = i * stride;
			auto end = std::min(count, (i + 1) * stride);

			for (auto j = start; j < end; ++j)
			{
				task(j);
			}
		}));
	}

	for (auto& f : threads)
	{
		f.wait();
	}
}

int CalTaskGroupStride(uint32_t total_count, int num_group )
{
	if (num_group == 0)
	{
		num_group = std::thread::hardware_concurrency() + 1;
	}

	return ALIGN(total_count, num_group) / num_group;
}