#pragma once

#include <functional>
#include <future>
#include <cstdint>

extern int CalTaskGroupStride(uint32_t total_count, int num_group = 0);
extern void ParallelTask(std::function<void(int)>&& task, uint32_t count);
extern void ParallelTask(uint64_t stride, uint64_t total, std::function<void(uint64_t, uint64_t)>&& task);
