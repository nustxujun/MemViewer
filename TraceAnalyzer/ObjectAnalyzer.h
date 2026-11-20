#pragma once
#include "CoreTypes.h"
#include <vector>
#include <string>
#include <memory>

struct ObjectNode {
    using Ptr = std::shared_ptr<ObjectNode>;
    std::string name;
    std::vector<Ptr> children;
    std::vector<const ObjectInfo*> objects;
    int64_t total_add = 0;
    int64_t total_delete = 0;
};

struct ObjectClassSummary {
    std::string class_name;
    std::vector<std::string> class_chain;
    int64_t add_count = 0;
    int64_t delete_count = 0;
};

class ObjectAnalyzer {
public:
    static ObjectNode::Ptr
    BuildClassTree(const std::vector<ObjectInfo>& objects,
                   const std::string& filter = "");

    static ObjectNode::Ptr
    BuildStackTree(const std::vector<ObjectInfo>& objects,
                   const Calltree::Ptr& calltree,
                   const std::string& filter = "");

    static std::vector<ObjectClassSummary>
    GetClassSummary(const std::vector<ObjectInfo>& objects,
                    int limit = 50, int offset = 0);

    static std::vector<const ObjectInfo*>
    GetObjectsByClass(const std::vector<ObjectInfo>& objects,
                      const std::string& class_filter,
                      int limit = 50, int offset = 0);

    static std::vector<std::string>
    GetObjectCallstack(const ObjectInfo& obj);

    static std::vector<std::string>
    GetObjectClassChain(const ObjectInfo& obj);
};
