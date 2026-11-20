#include "Concurrency.h"
#include "Utils.h"
#include <thread>
#include <atomic>
#include <vector>

void ParallelTask(std::function<void(int)>&& task, uint32_t count)
{
	std::vector<std::future<void>> threads;
	auto num_threads = std::thread::hardware_concurrency();
	uint32_t stride = count / num_threads + (count % num_threads ? 1 : 0);

	threads.reserve(num_threads);
	for (uint32_t i = 0; i < num_threads; ++i)
	{
		threads.push_back(std::async([=, &task]() {
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

void ParallelTask(uint64_t stride, uint64_t total, std::function<void(uint64_t, uint64_t)>&& task)
{
	std::vector<std::future<void>> threads;
	auto num_threads = std::thread::hardware_concurrency();

	stride = std::min(stride, total);
	std::atomic_uint64_t begin_task = 0;
	threads.reserve(num_threads);
	for (uint32_t i = 0; i < num_threads; ++i)
	{
		threads.push_back(std::async([&]() {
			while (begin_task < total)
			{
				auto begin = begin_task.fetch_add(stride);
				auto end = std::min(total, begin + stride);
				task(begin, end);
			}
		}));
	}

	for (auto& f : threads)
	{
		f.wait();
	}
}

int CalTaskGroupStride(uint32_t total_count, int num_group)
{
	if (num_group == 0)
	{
		num_group = std::thread::hardware_concurrency() + 1;
	}
	return ALIGN(total_count, num_group) / num_group;
}
