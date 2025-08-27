#pragma once

#include <functional>
#include <future>
#include <string>
#include <thread>
#include <mutex>
#include <list>

using Task = std::future<std::function<void()>>;
//struct TaskInfo
//{
//	Task task;
//	std::string name;
//	bool need_waitting;
//};


extern void AddTask(Task&& task);
extern void ShowWaitingTaskWindow();

template<class TaskFunc, class Callback>
static void AsyncTask(std::string name, TaskFunc&& task, Callback&& callback)
{
	Task future = std::async([task = std::move(task), callback = std::move(callback)]() {
		std::function<void()> cb = [result = std::move(task()), callback = std::move(callback)]() {
			callback(std::move(result));
		};

		return cb;
	});
	AddTask(std::move(future));
}

template<class TaskFunc>
static void AsyncTask(std::string name, TaskFunc&& task)
{
	Task future = std::async([task = std::move(task)]() {
		task();
		std::function<void()> cb = [](){};
		return cb;
	});
	AddTask(std::move(future));
}


extern void ProcessTasks();

extern int CalTaskGroupStride(uint32_t total_count, int num_group);
extern void ParallelTask(std::function<void(int)>&& task, uint32_t count);


