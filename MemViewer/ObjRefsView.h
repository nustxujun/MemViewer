#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>

struct UObjectRef
{
    using Ptr = std::shared_ptr<UObjectRef>;
    std::string classType;
    std::string path;
    std::vector<Ptr> parents; 
};

struct RefTree
{
    struct Node
    {
        using Ptr = std::shared_ptr<Node>;
        UObjectRef::Ptr obj;
        Node* parent = 0;
        std::vector<Node::Ptr> children;
        int depth = 0;
        bool bcycle = false;
        Node(UObjectRef::Ptr o, Node* p):
            obj(o),parent(p)
        {

        }

        static Node::Ptr Create(UObjectRef::Ptr o, Node* p = 0)
        {
            return std::make_shared<Node>(o,p);
        }
    };

    Node::Ptr root;

};
class ObjRefsView
{
public:
    ObjRefsView();
    ~ObjRefsView() = default;
    
    void Show();
    
private:
    void OpenFile();
    void ParseFile(const std::string& filePath);
    
    std::string openFileDialog();
    
    // 数据存储
    std::map<std::string, std::vector<UObjectRef::Ptr>> typeToObjects; // 类型 -> 对象列表
    std::unordered_map<std::string, UObjectRef::Ptr> pathToObject; // 路径 -> 对象

    RefTree reftree;
    
    // UI状态
    bool windowOpen = false;
    std::string selectedType;
    UObjectRef::Ptr selectedObject;
    std::string currentFilePath;
    
    // 搜索功能
    char typeSearchFilter[256] = {};
    char objectSearchFilter[256] = {};
    char refTreeSearchFilter[256] = {};
    bool ignoreCase = true; // 默认忽略大小写
    
    // 辅助函数：字符串匹配（支持忽略大小写）
    bool MatchString(const std::string& text, const std::string& pattern, bool ignoreCase) const;
    
    // 计算单个对象的最大深度（递归）
    int CalculateMaxDepth(const std::shared_ptr<UObjectRef>& obj, std::unordered_set<std::string>& visited, int currentDepth = 0);
    
    // 缓存所有对象的排序后的parents列表（在计算深度后调用）
    void CacheSortedParents();
    
    // 为单个对象缓存排序后的parents列表
    void CacheSortedParentsForObject(const std::shared_ptr<UObjectRef>& obj);
    
    // 显示引用树，返回是否有匹配的子节点
    bool ShowRefTree( RefTree::Node::Ptr obj);
};

