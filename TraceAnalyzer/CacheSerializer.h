#pragma once
#include "CoreTypes.h"
#include "TimelineCalculator.h"
#include "CategoryAnalyzer.h"
#include <string>
#include <cstdint>

class CacheSerializer {
public:
    static bool SerializeCache(const TraceData& data,
                               const PrecomputedData& precomputed,
                               const std::string& output_path);
    static bool DeserializeCache(const std::string& input_path,
                                 TraceData* data,
                                 PrecomputedData* precomputed);
    static bool IsCacheValid(const std::string& memtrace_path,
                             const std::string& cache_path);
    static uint64_t ComputeSourceHash(const std::string& path);
};
