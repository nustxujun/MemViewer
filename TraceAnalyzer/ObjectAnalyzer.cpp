#include "ObjectAnalyzer.h"
#include "Utils.h"
#include <algorithm>
#include <unordered_map>
#include <map>

ObjectNode::Ptr ObjectAnalyzer::BuildClassTree(
    const std::vector<ObjectInfo>& objects,
    const std::string& filter)
{
    auto root = std::make_shared<ObjectNode>();
    root->name = "root";

    auto add_to_tree = [](auto node, const ObjectInfo& obj, int depth, auto& self) -> void {
        if (depth >= (int)obj.class_chain->name_chain.size())
        {
            node->objects.push_back(&obj);
            return;
        }

        auto& cur_type = obj.class_chain->name_chain[depth];
        for (auto& c : node->children)
        {
            if (cur_type == c->name)
            {
                self(c, obj, depth + 1, self);
                return;
            }
        }

        auto new_child = std::make_shared<ObjectNode>();
        new_child->name = cur_type;
        node->children.push_back(new_child);
        self(new_child, obj, depth + 1, self);
    };

    for (auto& obj : objects)
        add_to_tree(root, obj, 0, add_to_tree);

    auto reorder = [&filter](auto node, auto& self) -> void {
        node->total_add = 0;
        node->total_delete = 0;

        auto iter = node->children.begin();
        for (; iter != node->children.end();)
        {
            self(*iter, self);
            node->total_add += (*iter)->total_add;
            node->total_delete += (*iter)->total_delete;

            if (!filter.empty() &&
                (*iter)->total_add == 0 && (*iter)->total_delete == 0 &&
                (*iter)->name.find(filter) == std::string::npos)
            {
                iter = node->children.erase(iter);
            }
            else
            {
                iter++;
            }
        }

        auto obj_iter = node->objects.begin();
        for (; obj_iter != node->objects.end();)
        {
            if (!filter.empty() &&
                (*obj_iter)->name.find(filter) == std::string::npos &&
                node->name.find(filter) == std::string::npos)
            {
                obj_iter = node->objects.erase(obj_iter);
            }
            else
            {
                obj_iter++;
            }
        }

        for (auto& obj : node->objects)
        {
            if (obj->state >= 0)
                node->total_add++;
            else
                node->total_delete++;
        }

        std::stable_sort(node->objects.begin(), node->objects.end(), [](auto& a, auto& b) {
            if (a->state != b->state)
                return a->state > b->state;
            return a->name < b->name;
        });

        std::stable_sort(node->children.begin(), node->children.end(), [](auto& a, auto& b) {
            if (a->total_add != b->total_add)
                return a->total_add > b->total_add;
            return a->total_delete < b->total_delete;
        });
    };

    reorder(root, reorder);
    return root;
}

ObjectNode::Ptr ObjectAnalyzer::BuildStackTree(
    const std::vector<ObjectInfo>& objects,
    const Calltree::Ptr& calltree,
    const std::string& filter)
{
    auto root = std::make_shared<ObjectNode>();
    root->name = "root";

    std::map<int, ObjectNode::Ptr> node_map;

    auto build_stack_nodes = [&](auto parent, Node* tree_node, auto& self) -> void {
        for (auto& c : tree_node->children)
        {
            auto new_child = std::make_shared<ObjectNode>();
            new_child->name = c->name;
            node_map.emplace(c.index, new_child);
            parent->children.push_back(new_child);
            self(new_child, c.getNode(), self);
        }
    };

    build_stack_nodes(root, calltree->root, build_stack_nodes);

    for (auto& obj : objects)
    {
        auto it = node_map.find(obj.node.index);
        if (it != node_map.end())
            it->second->objects.push_back(&obj);
    }

    auto reorder = [&filter](auto node, auto& self) -> void {
        node->total_add = 0;
        node->total_delete = 0;

        auto iter = node->children.begin();
        for (; iter != node->children.end();)
        {
            self(*iter, self);
            node->total_add += (*iter)->total_add;
            node->total_delete += (*iter)->total_delete;

            if ((*iter)->total_add == 0 && (*iter)->total_delete == 0 && (*iter)->objects.empty())
            {
                iter = node->children.erase(iter);
            }
            else
            {
                iter++;
            }
        }

        for (auto& obj : node->objects)
        {
            if (obj->state >= 0)
                node->total_add++;
            else
                node->total_delete++;
        }

        std::stable_sort(node->children.begin(), node->children.end(), [](auto& a, auto& b) {
            if (a->total_add != b->total_add)
                return a->total_add > b->total_add;
            return a->total_delete < b->total_delete;
        });
    };

    reorder(root, reorder);
    return root;
}

std::vector<ObjectClassSummary> ObjectAnalyzer::GetClassSummary(
    const std::vector<ObjectInfo>& objects,
    int limit, int offset)
{
    struct ClassStats {
        std::vector<std::string> class_chain;
        int64_t add_count = 0;
        int64_t delete_count = 0;
    };

    std::unordered_map<std::string, ClassStats> class_map;

    for (auto& obj : objects)
    {
        if (!obj.class_chain || obj.class_chain->name_chain.empty())
            continue;

        auto& deepest = obj.class_chain->name_chain.back();
        auto& stats = class_map[deepest];
        if (stats.class_chain.empty())
            stats.class_chain = obj.class_chain->name_chain;

        if (obj.state >= 0)
            stats.add_count++;
        else
            stats.delete_count++;
    }

    std::vector<ObjectClassSummary> result;
    result.reserve(class_map.size());
    for (auto& [name, stats] : class_map)
    {
        result.push_back({name, stats.class_chain, stats.add_count, stats.delete_count});
    }

    std::stable_sort(result.begin(), result.end(), [](auto& a, auto& b) {
        return a.add_count > b.add_count;
    });

    if (offset >= (int)result.size())
        return {};

    auto begin = result.begin() + offset;
    auto end = (offset + limit <= (int)result.size()) ? begin + limit : result.end();
    return {begin, end};
}

std::vector<const ObjectInfo*> ObjectAnalyzer::GetObjectsByClass(
    const std::vector<ObjectInfo>& objects,
    const std::string& class_filter,
    int limit, int offset)
{
    std::vector<const ObjectInfo*> matched;

    for (auto& obj : objects)
    {
        if (!obj.class_chain)
            continue;

        for (auto& name : obj.class_chain->name_chain)
        {
            if (name.find(class_filter) != std::string::npos)
            {
                matched.push_back(&obj);
                break;
            }
        }
    }

    if (offset >= (int)matched.size())
        return {};

    auto begin = matched.begin() + offset;
    auto end = (offset + limit <= (int)matched.size()) ? begin + limit : matched.end();
    return {begin, end};
}

std::vector<std::string> ObjectAnalyzer::GetObjectCallstack(const ObjectInfo& obj)
{
    std::vector<std::string> result;
    auto node = obj.node;
    while (node)
    {
        result.push_back(node->name);
        node = node->parent;
    }
    return result;
}

std::vector<std::string> ObjectAnalyzer::GetObjectClassChain(const ObjectInfo& obj)
{
    if (!obj.class_chain)
        return {};
    return obj.class_chain->name_chain;
}
