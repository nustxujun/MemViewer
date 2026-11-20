#pragma once
#include "CoreTypes.h"
#include <vector>
#include <string>

struct CategoryConfig {
    struct SubCategory {
        std::string name;
        std::vector<std::string> keys;
    };
    struct Category {
        std::string name;
        std::vector<std::string> keys;
        std::vector<SubCategory> subs;
    };
};

struct CategoryMapping {
    int16_t category_index = -1;      // -1 = Untagged
    int32_t matched_node_index = -1;
};

struct CategoryAllocInfo {
    int node_index;
    int matched_node_index;
    int64_t count;
    int64_t size;
};

struct CategoryResult {
    std::string name;
    int64_t total_size = 0;
    int64_t total_count = 0;
    std::vector<CategoryAllocInfo> allocs;
    std::vector<CategoryResult> sub_results;
};

class CategoryAnalyzer {
public:
    static std::vector<CategoryConfig::Category>
    ParseConfig(const std::string& file_path);

    static std::vector<CategoryMapping>
    BuildCategoryMap(const Calltree::Ptr& calltree,
                     const std::vector<CategoryConfig::Category>& categories);

    static std::vector<CategoryResult>
    AggregateByMap(const Calltree::Ptr& calltree,
                   const std::vector<CategoryMapping>& category_map,
                   const std::vector<CategoryConfig::Category>& categories);

    static std::pair<std::vector<CategoryResult>, std::vector<CategoryMapping>>
    AnalyzeAndBuildMap(const Calltree::Ptr& calltree,
                      const std::vector<CategoryConfig::Category>& categories);

    static std::vector<CategoryMapping>
    BuildSubCategoryMap(const Calltree::Ptr& calltree,
                        const std::vector<CategoryAllocInfo>& parent_allocs,
                        const std::vector<CategoryConfig::SubCategory>& sub_cats);

    static std::vector<CategoryResult>
    AggregateSubByMap(const Calltree::Ptr& calltree,
                      const std::vector<CategoryAllocInfo>& parent_allocs,
                      const std::vector<CategoryMapping>& sub_map,
                      const std::vector<CategoryConfig::SubCategory>& sub_cats);

    static int MatchCategory(const std::string& name,
                             const std::vector<CategoryConfig::Category>& cats);
};
