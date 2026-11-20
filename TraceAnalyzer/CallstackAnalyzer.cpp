#include "CallstackAnalyzer.h"
#include "Utils.h"
#include <algorithm>
#include <queue>

static CallstackNodeInfo makeNodeInfo(const Calltree::Ptr& tree, int idx) {
    auto* node = tree->get(idx);
    return {
        idx,
        node->name,
        node->size,
        node->basic_size,
        node->count,
        node->basic_count,
        node->is_matched,
        (int)node->children.size()
    };
}

static void CalSize(const Calltree::Ptr& tree,
                    Node* node,
                    bool ignore_node,
                    const std::vector<std::string>& filters,
                    bool case_sensitive)
{
    node->size = 0;
    node->count = 0;
    node->is_matched = false;

    if (ignore_node && !filters.empty())
    {
        std::string name = case_sensitive ? node->name : to_lower(node->name);
        for (auto& filter : filters)
        {
            ignore_node = name.find(filter) == std::string::npos;
            if (!ignore_node)
                break;
        }
        node->is_matched = !ignore_node;
    }

    for (auto& child : node->children)
    {
        CalSize(tree, child.getNode(), ignore_node, filters, case_sensitive);
        node->size += child->size;
        node->count += child->count;
    }

    std::stable_sort(node->children.begin(), node->children.end(), [](auto& n1, auto& n2) {
        return n1->size > n2->size;
    });

    if (!ignore_node)
    {
        node->size += node->basic_size;
        node->count += node->basic_count;
    }
}

Calltree::Ptr CallstackAnalyzer::BuildFilteredTree(
    const Calltree::Ptr& source,
    const std::vector<std::string>& filters,
    bool case_sensitive)
{
    auto cloned = source->clone();

    std::vector<std::string> normalized_filters = filters;
    if (!case_sensitive)
    {
        for (auto& f : normalized_filters)
            f = to_lower(f);
    }

    CalSize(cloned, cloned->get(0), !normalized_filters.empty(), normalized_filters, case_sensitive);
    return cloned;
}

std::vector<CallstackNodeInfo> CallstackAnalyzer::GetTopNodes(
    const Calltree::Ptr& tree,
    int depth, int64_t min_size, int limit)
{
    std::vector<CallstackNodeInfo> result;

    struct BfsItem {
        int node_index;
        int current_depth;
    };

    std::queue<BfsItem> q;
    q.push({0, 0});

    while (!q.empty() && (int)result.size() < limit)
    {
        auto [idx, d] = q.front();
        q.pop();

        auto* node = tree->get(idx);

        if (std::abs(node->size) >= min_size && idx != 0)
            result.push_back(makeNodeInfo(tree, idx));

        if (d < depth)
        {
            for (auto& child : node->children)
                q.push({child.index, d + 1});
        }
    }

    std::stable_sort(result.begin(), result.end(), [](auto& a, auto& b) {
        return std::abs(a.size) > std::abs(b.size);
    });

    if ((int)result.size() > limit)
        result.resize(limit);

    return result;
}

std::vector<CallstackNodeInfo> CallstackAnalyzer::GetChildren(
    const Calltree::Ptr& tree, int node_index)
{
    std::vector<CallstackNodeInfo> result;
    if (node_index < 0 || node_index >= tree->node_count)
        return result;

    auto* node = tree->get(node_index);
    result.reserve(node->children.size());
    for (auto& child : node->children)
        result.push_back(makeNodeInfo(tree, child.index));

    return result;
}

std::vector<CallstackNodeInfo> CallstackAnalyzer::GetPath(
    const Calltree::Ptr& tree, int node_index)
{
    std::vector<CallstackNodeInfo> result;
    if (node_index < 0 || node_index >= tree->node_count)
        return result;

    int idx = node_index;
    while (true)
    {
        result.push_back(makeNodeInfo(tree, idx));
        auto* node = tree->get(idx);
        if (!node->parent)
            break;
        idx = node->parent.index;
    }

    return result;
}

std::vector<CallstackNodeInfo> CallstackAnalyzer::Search(
    const Calltree::Ptr& tree,
    const std::vector<std::string>& keywords,
    bool case_sensitive,
    int limit)
{
    std::vector<std::string> normalized_keys = keywords;
    if (!case_sensitive)
    {
        for (auto& k : normalized_keys)
            k = to_lower(k);
    }

    std::vector<CallstackNodeInfo> result;
    for (int i = 0; i < tree->node_count; ++i)
    {
        auto* node = tree->get(i);
        std::string name = case_sensitive ? node->name : to_lower(node->name);

        for (auto& key : normalized_keys)
        {
            if (name.find(key) != std::string::npos)
            {
                result.push_back(makeNodeInfo(tree, i));
                break;
            }
        }
    }

    std::stable_sort(result.begin(), result.end(), [](auto& a, auto& b) {
        return std::abs(a.size) > std::abs(b.size);
    });

    if ((int)result.size() > limit)
        result.resize(limit);

    return result;
}

CallstackNodeInfo CallstackAnalyzer::GetNodeInfo(
    const Calltree::Ptr& tree, int node_index)
{
    if (node_index < 0 || node_index >= tree->node_count)
        return {-1, "", 0, 0, 0, 0, false, 0};

    return makeNodeInfo(tree, node_index);
}
