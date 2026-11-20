#pragma once
#include "CoreTypes.h"
#include "CategoryAnalyzer.h"
#include <vector>
#include <string>

struct TimelineSeries {
    std::string name;
    std::vector<float> data;
    enum Type { Bar, Line } type = Bar;
    bool stacking = false;
};

struct TimelineData {
    std::vector<TimelineSeries> series;
    FrameRange frame_range;
    int frame_count = 0;
};

struct PrecomputedData {
    std::vector<TimelineSeries> basic_timelines;    // trk, phy, lua, custom
    std::vector<TimelineSeries> category_timelines;  // default config
    std::vector<CategoryMapping> native_category_map;
    std::vector<CategoryMapping> lua_category_map;
    std::vector<std::vector<CategoryMapping>> native_subcategory_maps;
};

class TimelineCalculator {
public:
    static TimelineSeries
    CalcTrackedMemory(const std::vector<AllocInfo>& allocs, FrameRange range);

    static TimelineSeries
    CalcPhysicalMemory(const std::vector<TotalInfo>& memoryinfos);

    static TimelineSeries
    CalcLuaMemory(const std::vector<AllocInfo>& lua_allocs, FrameRange range);

    static std::vector<TimelineSeries>
    CalcCustomData(const std::vector<TotalInfo>& memoryinfos);

    static std::vector<TimelineSeries>
    CalcCategoryTimeline(const std::vector<AllocInfo>& allocs,
                         const Calltree::Ptr& calltree,
                         const std::vector<CategoryConfig::Category>& categories,
                         const std::vector<CategoryMapping>& category_map,
                         FrameRange range);

    static std::vector<float>
    Downsample(const std::vector<float>& data, int target_count);

    static TimelineData
    CalcAll(const TraceData& data);
};
