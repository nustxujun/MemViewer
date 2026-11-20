#include "Concurrency.h"
#include "Utils.h"
#include <mutex>

std::vector<Task> task_list;
std::mutex mutex;

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
		try
		{
			task.get()();
		}
		catch (const std::exception& e)
		{
			Log("Task failed: {}", e.what());
		}
	}

}

void ShowWaitingTaskWindow()
{
}
