#pragma once
#include "CoreTypes.h"
#include <vector>
#include <string>

struct CallstackNodeInfo {
    int node_index;
    std::string name;
    int64_t size;
    int64_t basic_size;
    int32_t count;
    int32_t basic_count;
    bool is_matched = false;
    int children_count;
};

class CallstackAnalyzer {
public:
    static Calltree::Ptr
    BuildFilteredTree(const Calltree::Ptr& source,
                      const std::vector<std::string>& filters = {},
                      bool case_sensitive = false);

    static std::vector<CallstackNodeInfo>
    GetTopNodes(const Calltree::Ptr& tree,
                int depth = 3, int64_t min_size = 0, int limit = 50);

    static std::vector<CallstackNodeInfo>
    GetChildren(const Calltree::Ptr& tree, int node_index);

    static std::vector<CallstackNodeInfo>
    GetPath(const Calltree::Ptr& tree, int node_index);

    static std::vector<CallstackNodeInfo>
    Search(const Calltree::Ptr& tree,
           const std::vector<std::string>& keywords,
           bool case_sensitive = false,
           int limit = 50);

    static CallstackNodeInfo
    GetNodeInfo(const Calltree::Ptr& tree, int node_index);
};
