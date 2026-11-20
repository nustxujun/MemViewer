#include "TimelineCalculator.h"
#include "Concurrency.h"
#include "Utils.h"

#include <mutex>
#include <atomic>
#include <algorithm>
#include <map>

TimelineSeries
TimelineCalculator::CalcTrackedMemory(const std::vector<AllocInfo>& allocs, FrameRange range)
{
	int count = range.count() + 1;
	std::vector<int64_t> datas(count, 0);
	std::mutex mutex;

	uint64_t alloc_count = allocs.size();
	ParallelTask(10000, alloc_count, [&](uint64_t begin, uint64_t end) {
		std::vector<int64_t> local_datas(count, 0);
		for (auto i = begin; i < end; ++i)
		{
			auto& alloc = allocs[i];
			auto end_frame = std::min<uint32_t>(alloc.end, range.end);
			for (uint32_t f = alloc.start; f < end_frame; ++f)
			{
				local_datas[f - range.begin] += alloc.size;
			}
		}

		std::lock_guard<std::mutex> lock(mutex);
		for (int i = 0; i < count; ++i)
			datas[i] += local_datas[i];
	});

	const float inv = 1.0f / 1024.0f / 1024.0f;
	TimelineSeries series;
	series.name = "trk";
	series.type = TimelineSeries::Bar;
	series.data.reserve(count);
	for (int i = 0; i < count; ++i)
		series.data.push_back(datas[i] * inv);

	return series;
}

TimelineSeries
TimelineCalculator::CalcPhysicalMemory(const std::vector<TotalInfo>& memoryinfos)
{
	TimelineSeries series;
	series.name = "phy";
	series.type = TimelineSeries::Line;
	series.data.reserve(memoryinfos.size());
	for (auto& info : memoryinfos)
		series.data.push_back(info.used / 1024.0f / 1024.0f);
	return series;
}

TimelineSeries
TimelineCalculator::CalcLuaMemory(const std::vector<AllocInfo>& lua_allocs, FrameRange range)
{
	int count = range.count() + 1;
	std::vector<int64_t> datas(count, 0);
	std::mutex mutex;

	uint64_t alloc_count = lua_allocs.size();
	ParallelTask(10000, alloc_count, [&](uint64_t begin, uint64_t end) {
		std::vector<int64_t> local_datas(count, 0);
		for (auto i = begin; i < end; ++i)
		{
			auto& alloc = lua_allocs[i];
			auto end_frame = std::min<uint32_t>(alloc.end, range.end);
			for (uint32_t f = alloc.start; f < end_frame; ++f)
			{
				local_datas[f - range.begin] += alloc.size;
			}
		}

		std::lock_guard<std::mutex> lock(mutex);
		for (int i = 0; i < count; ++i)
			datas[i] += local_datas[i];
	});

	const float inv = 1.0f / 1024.0f / 1024.0f;
	TimelineSeries series;
	series.name = "lua";
	series.type = TimelineSeries::Bar;
	series.data.reserve(count);
	for (int i = 0; i < count; ++i)
		series.data.push_back(datas[i] * inv);

	return series;
}

std::vector<TimelineSeries>
TimelineCalculator::CalcCustomData(const std::vector<TotalInfo>& memoryinfos)
{
	std::map<std::string, std::vector<float>> custom_datas;

	uint32_t frame_index = 0;
	for (auto& info : memoryinfos)
	{
		frame_index++;
		for (auto& kv : info.custom_datas)
		{
			custom_datas[kv.first].resize(frame_index);
			custom_datas[kv.first][frame_index - 1] = kv.second / 1024.0f / 1024.0f;
		}
	}

	std::vector<TimelineSeries> result;
	for (auto& kv : custom_datas)
	{
		TimelineSeries series;
		series.name = kv.first;
		series.type = TimelineSeries::Bar;
		series.stacking = true;
		series.data = std::move(kv.second);
		result.push_back(std::move(series));
	}
	return result;
}

std::vector<TimelineSeries>
TimelineCalculator::CalcCategoryTimeline(const std::vector<AllocInfo>& allocs,
                                          const Calltree::Ptr& calltree,
                                          const std::vector<CategoryConfig::Category>& categories,
                                          const std::vector<CategoryMapping>& category_map,
                                          FrameRange range)
{
	int count = range.count() + 1;
	int cat_count = static_cast<int>(categories.size());

	std::vector<std::unique_ptr<std::atomic_int64_t[]>> cate_datas(cat_count);
	for (int i = 0; i < cat_count; ++i)
	{
		cate_datas[i] = std::make_unique<std::atomic_int64_t[]>(count);
		for (int j = 0; j < count; ++j)
			cate_datas[i][j].store(0, std::memory_order_relaxed);
	}

	uint64_t alloc_count = allocs.size();
	ParallelTask(5000, alloc_count, [&](uint64_t begin, uint64_t end) {
		for (auto i = begin; i < end; ++i)
		{
			auto& info = allocs[i];
			int node_idx = info.node.index;
			if (node_idx < 0 || node_idx >= static_cast<int>(category_map.size()))
				continue;

			int cat_idx = category_map[node_idx].category_index;
			if (cat_idx < 0 || cat_idx >= cat_count)
				continue;

			auto end_frame = std::min<uint32_t>(info.end, range.end);
			for (uint32_t f = info.start; f < end_frame; ++f)
				cate_datas[cat_idx][f - range.begin].fetch_add(info.size, std::memory_order_relaxed);
		}
	});

	const float inv = 1.0f / 1024.0f / 1024.0f;
	std::vector<TimelineSeries> result;
	for (int i = 0; i < cat_count; ++i)
	{
		TimelineSeries series;
		series.name = categories[i].name + "_category";
		series.type = TimelineSeries::Line;

		series.data.resize(count);
		for (int j = 0; j < count; ++j)
			series.data[j] = cate_datas[i][j].load(std::memory_order_relaxed) * inv;

		result.push_back(std::move(series));
	}
	return result;
}

std::vector<float>
TimelineCalculator::Downsample(const std::vector<float>& data, int target_count)
{
	if (target_count <= 0 || data.empty())
		return {};

	if (static_cast<int>(data.size()) <= target_count)
		return data;

	std::vector<float> result;
	result.reserve(target_count);

	float bucket_size = static_cast<float>(data.size()) / target_count;
	for (int i = 0; i < target_count; ++i)
	{
		int start = static_cast<int>(i * bucket_size);
		int end = static_cast<int>((i + 1) * bucket_size);
		end = std::min(end, static_cast<int>(data.size()));

		float max_val = data[start];
		for (int j = start + 1; j < end; ++j)
			max_val = std::max(max_val, data[j]);

		result.push_back(max_val);
	}
	return result;
}

TimelineData
TimelineCalculator::CalcAll(const TraceData& data)
{
	TimelineData timeline;
	timeline.frame_range = data.max_range;
	timeline.frame_count = data.max_range.count() + 1;

	auto trk = CalcTrackedMemory(data.allocs, data.max_range);
	timeline.series.push_back(std::move(trk));

	auto phy = CalcPhysicalMemory(data.memoryinfos);
	timeline.series.push_back(std::move(phy));

	if (!data.lua_allocs.empty())
	{
		auto lua = CalcLuaMemory(data.lua_allocs, data.max_range);
		timeline.series.push_back(std::move(lua));
	}

	auto custom = CalcCustomData(data.memoryinfos);
	for (auto& s : custom)
		timeline.series.push_back(std::move(s));

	return timeline;
}
