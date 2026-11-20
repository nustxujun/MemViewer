#include "CoreTypes.h"
#include "CategoryAnalyzer.h"
#include "TimelineCalculator.h"
#include "CacheSerializer.h"
#include "Utils.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <format>

int main(int argc, char* argv[]) {
    // Parse command line args
    if (argc < 2) {
        std::cerr << "Usage: exporter <input.memtrace> [--output <output.memcache>] [--config-dir <configs/>] [--force]" << std::endl;
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path;
    std::string config_dir = "configs";
    bool force = false;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) output_path = argv[++i];
        else if (arg == "--config-dir" && i + 1 < argc) config_dir = argv[++i];
        else if (arg == "--force") force = true;
    }

    // Default output path: same dir as input, with .memcache extension
    if (output_path.empty()) {
        std::filesystem::path p(input_path);
        output_path = (p.parent_path() / p.stem()).string() + ".memcache";
    }

    // Check if cache is valid (skip if --force)
    if (!force && CacheSerializer::IsCacheValid(input_path, output_path)) {
        std::cerr << "Cache is up to date: " << output_path << std::endl;
        // Output JSON summary to stdout
        std::cout << R"({"status": "cached", "path": ")" << output_path << R"("})" << std::endl;
        return 0;
    }

    // Step 1: Parse .memtrace
    std::cerr << "[1/4] Parsing .memtrace..." << std::endl;
    TraceData data;
    {
        Counter c("parse");
        if (!TraceData::ParseTraceFile(input_path, &data)) {
            std::cerr << "ERROR: Failed to parse " << input_path << std::endl;
            std::cout << R"({"status": "error", "message": "Failed to parse memtrace file"})" << std::endl;
            return 1;
        }
    }

    PrecomputedData precomputed;

    // Step 2: Build category maps
    std::cerr << "[2/4] Building category maps..." << std::endl;
    {
        Counter c("category maps");
        auto native_config_path = (std::filesystem::path(config_dir) / "category_config.ini").string();
        auto lua_config_path = (std::filesystem::path(config_dir) / "lua_category_config.ini").string();

        auto native_cats = CategoryAnalyzer::ParseConfig(native_config_path);
        auto lua_cats = CategoryAnalyzer::ParseConfig(lua_config_path);

        if (data.calltree) {
            precomputed.native_category_map = CategoryAnalyzer::BuildCategoryMap(data.calltree, native_cats);

            // Build subcategory maps for each native category
            auto native_results = CategoryAnalyzer::AggregateByMap(data.calltree, precomputed.native_category_map, native_cats);
            precomputed.native_subcategory_maps.resize(native_cats.size());
            for (size_t i = 0; i < native_cats.size(); i++) {
                if (!native_cats[i].subs.empty() && i < native_results.size()) {
                    precomputed.native_subcategory_maps[i] = CategoryAnalyzer::BuildSubCategoryMap(
                        data.calltree, native_results[i].allocs, native_cats[i].subs);
                }
            }
        }

        if (data.lua_calltree) {
            precomputed.lua_category_map = CategoryAnalyzer::BuildCategoryMap(data.lua_calltree, lua_cats);
        }
    }

    // Step 3: Compute timelines
    std::cerr << "[3/4] Computing timelines..." << std::endl;
    {
        Counter c("timelines");
        FrameRange range = data.max_range;

        // Basic timelines
        precomputed.basic_timelines.push_back(TimelineCalculator::CalcTrackedMemory(data.allocs, range));
        precomputed.basic_timelines.push_back(TimelineCalculator::CalcPhysicalMemory(data.memoryinfos));
        precomputed.basic_timelines.push_back(TimelineCalculator::CalcLuaMemory(data.lua_allocs, range));

        auto customs = TimelineCalculator::CalcCustomData(data.memoryinfos);
        for (auto& c : customs)
            precomputed.basic_timelines.push_back(std::move(c));

        // Category timelines (using pre-computed map)
        if (data.calltree && !precomputed.native_category_map.empty()) {
            auto native_config_path = (std::filesystem::path(config_dir) / "category_config.ini").string();
            auto native_cats = CategoryAnalyzer::ParseConfig(native_config_path);
            precomputed.category_timelines = TimelineCalculator::CalcCategoryTimeline(
                data.allocs, data.calltree, native_cats, precomputed.native_category_map, range);
        }
    }

    // Step 4: Write cache
    std::cerr << "[4/4] Writing cache..." << std::endl;
    {
        Counter c("write cache");
        if (!CacheSerializer::SerializeCache(data, precomputed, output_path)) {
            std::cerr << "ERROR: Failed to write cache to " << output_path << std::endl;
            std::cout << R"({"status": "error", "message": "Failed to write cache file"})" << std::endl;
            return 1;
        }
    }

    // Output JSON summary
    auto frame_count = data.max_range.end - data.max_range.begin;
    std::cout << std::format(
        R"({{"status": "ok", "path": "{}", "frames": {}, "allocs": {}, "nodes": {}, "objects": {}, "rhis": {}}})",
        output_path, frame_count, data.allocs.size(),
        data.calltree ? data.calltree->node_count : 0,
        data.objects.size(), data.rhis.size()
    ) << std::endl;

    std::cerr << "Done! Output: " << output_path << std::endl;
    return 0;
}
