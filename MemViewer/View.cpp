#include "View.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "TraceInstance.h"


#include <list>
#include "Concurrency.h"

using ViewTask = std::future<std::function<void()>>;
//static std::list<ViewTask> tasks;

static void process_tasks(bool stop = false)
{

}

View::View(std::string n):name(n)
{

}

void View::Initialize() 
{
	AddTask([this]() {
		InitializeImpl();
		});
}

void View::Update()
{
	AddTask([this]() {
		UpdateImpl();
	});
}

void View::Show(float w, float h, int flags, std::function<bool(TraceInstanceBasic*)>&& cond)
{
	ImGui::BeginChild(name.c_str(),{w,h},flags);


	if (!cond)
	{
		cond = [](auto trace){
			return trace && trace->isPrepared();
		};
	}

	if (!isRunningTask() && cond(trace.get()))
	{
		ShowImpl();
	}
	else
	{
		process_tasks();
		auto Space = ImGui::GetContentRegionAvail();
		ImGui::SetCursorPosX(Space.x / 2 - 3);
		ImGui::SetCursorPosY(Space.y / 2 - 3);

		Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
	}
	ImGui::EndChild();
}

void View::Process()
{


}
void View::syncTask(bool cancel)
{
	require_stop = true;
	while(isRunningTask())
		std::this_thread::yield();
	require_stop = false;
}
bool View::isRunningTask()
{
	auto num_tasks = 0;
	{
		std::lock_guard<std::mutex> lock(mutex);
		num_tasks = tasks.size();
	}
	bool running = exec_tasks.valid() && exec_tasks.wait_for(std::chrono::nanoseconds(0)) != std::future_status::ready;
	if (num_tasks == 0 && !running)
		return false;

	if (!running)
	{
		exec_tasks = std::async([this]() {

			while(true)
			{
				std::vector<std::function<void()>> internal_tasks;
				{
					std::lock_guard<std::mutex> lock(mutex);
					internal_tasks = std::move(tasks);
					if (internal_tasks.size() == 0)
						break;
				}

				for (auto& task : internal_tasks)
				{
					task();
				}
			}
		});

	}
	return true;
}


void View::AddTask(std::function<void()>&& func)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		tasks.push_back(std::move(func));
	}


	isRunningTask();

}

