#include "ObjRefsView.h"
#include "imgui/imgui.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <commdlg.h>
#include <format>
#include <cctype>
#include <execution>
#undef max
#undef min
ObjRefsView::ObjRefsView()
{
    memset(typeSearchFilter, 0, sizeof(typeSearchFilter));
    memset(objectSearchFilter, 0, sizeof(objectSearchFilter));
    memset(refTreeSearchFilter, 0, sizeof(refTreeSearchFilter));
}

bool ObjRefsView::MatchString(const std::string& text, const std::string& pattern, bool ignoreCase) const
{
    if (pattern.empty())
        return true;
    
    if (ignoreCase)
    {
        // 转换为小写进行比较
        std::string textLower = text;
        std::string patternLower = pattern;
        std::transform(textLower.begin(), textLower.end(), textLower.begin(), ::tolower);
        std::transform(patternLower.begin(), patternLower.end(), patternLower.begin(), ::tolower);
        return textLower.find(patternLower) != std::string::npos;
    }
    else
    {
        return text.find(pattern) != std::string::npos;
    }
}

std::string ObjRefsView::openFileDialog()
{
    char cur_dir[MAX_PATH];
    ::GetCurrentDirectoryA(MAX_PATH, cur_dir);

    OPENFILENAMEA open;
    ZeroMemory(&open, sizeof(open));

    char path[MAX_PATH] = {};

    open.lStructSize = sizeof(open);
    open.hwndOwner = NULL;
    open.lpstrFile = path;
    open.nMaxFile = sizeof(path);
    open.lpstrFilter = "All Files\0*.*\0Text Files\0*.txt\0";
    open.nFilterIndex = 1;
    open.lpstrFileTitle = NULL;
    open.nMaxFileTitle = 0;
    open.lpstrInitialDir = NULL;
    open.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    auto selected = GetOpenFileNameA(&open);
    auto ret = GetLastError();

    ::SetCurrentDirectoryA(cur_dir);

    if (selected)
        return std::string(path);
    return "";
}

void ObjRefsView::ParseFile(const std::string& filePath)
{
    typeToObjects.clear();
    pathToObject.clear();
    selectedType.clear();
    selectedObject.reset();
    
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return;
    }
    
    std::string line;
    std::shared_ptr<UObjectRef> currentParent = nullptr;
    
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;
        
        // 检查是否是子对象（以空格或制表符开头）
        bool isChild = (line[0] == ' ' || line[0] == '\t');
        
        // 去除前后空白
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
            continue;
        size_t end = line.find_last_not_of(" \t");
        line = line.substr(start, end - start + 1);
        
        if (line.empty())
            continue;
        
        if (isChild)
        {
            // 子对象
            std::string childPath = line;
            // 去除前导空格/制表符
            size_t firstNonSpace = childPath.find_first_not_of(" \t");
            if (firstNonSpace != std::string::npos)
                childPath = childPath.substr(firstNonSpace);
            
            // 移除 "(child)" 标记（如果存在）
            size_t childPos = childPath.find("(child)");
            if (childPos != std::string::npos)
            {
                childPath = childPath.substr(0, childPos);
                // 去除尾部空格
                size_t lastNonSpace = childPath.find_last_not_of(" \t");
                if (lastNonSpace != std::string::npos)
                    childPath = childPath.substr(0, lastNonSpace + 1);
            }
            
            if (currentParent && !childPath.empty())
            {
                // 解析子对象的 ClassType /Path 格式
                size_t slashPos = childPath.find(" /");
                std::string classType;
                std::string path;
                
                if (slashPos != std::string::npos)
                {
                    classType = childPath.substr(0, slashPos);
                    path = childPath.substr(slashPos + 1); // 包含前导空格
                    // 去除前导空格
                    size_t pathStart = path.find_first_not_of(" \t");
                    if (pathStart != std::string::npos)
                        path = path.substr(pathStart);
                }
                else
                {
                    // 如果没有找到 /，整个字符串作为path
                    path = childPath;
                }
                
                if (!path.empty())
                {
                    // 获取或创建子对象
                    std::shared_ptr<UObjectRef> childObj;
                    if (pathToObject.find(path) == pathToObject.end())
                    {
                        childObj = std::make_shared<UObjectRef>();
                        childObj->path = path;
                        childObj->classType = classType;
                        pathToObject[path] = childObj;
                        
                        // 添加到类型映射
                        typeToObjects[classType].push_back(childObj);
                    }
                    else
                    {
                        childObj = pathToObject[path];
                    }
                    
                    // 添加引用关系：currentParent 引用了 childObj
                    // 所以childObj的parents列表中添加currentParent的path
                    childObj->parents.push_back(currentParent);
                }
            }
        }
        else
        {
            // 父对象
            std::string parentLine = line;
            
            // 移除 "(parent)" 标记（如果存在）
            size_t parentPos = parentLine.find("(parent)");
            if (parentPos != std::string::npos)
            {
                parentLine = parentLine.substr(0, parentPos);
                // 去除尾部空格
                size_t lastNonSpace = parentLine.find_last_not_of(" \t");
                if (lastNonSpace != std::string::npos)
                    parentLine = parentLine.substr(0, lastNonSpace + 1);
            }
            
            // 解析 ClassType /Path 格式
            size_t slashPos = parentLine.find(" /");
            std::string classType;
            std::string path;
            
            if (slashPos != std::string::npos)
            {
                classType = parentLine.substr(0, slashPos);
                path = parentLine.substr(slashPos + 1); // 包含前导空格
                // 去除前导空格
                size_t pathStart = path.find_first_not_of(" \t");
                if (pathStart != std::string::npos)
                    path = path.substr(pathStart);
            }
            else
            {
                // 如果没有找到 /，整个字符串作为path
                path = parentLine;
            }
            
            if (!path.empty())
            {
                // 获取或创建父对象
                if (pathToObject.find(path) == pathToObject.end())
                {
                    currentParent = std::make_shared<UObjectRef>();
                    currentParent->path = path;
                    currentParent->classType = classType;
                    pathToObject[path] = currentParent;
                    
                    // 添加到类型映射
                    typeToObjects[classType].push_back(currentParent);
                }
                else
                {
                    currentParent = pathToObject[path];
                }
            }
        }
    }
    
    file.close();
    currentFilePath = filePath;
    

}

void ObjRefsView::OpenFile()
{
    std::string filePath = openFileDialog();
    if (!filePath.empty())
    {
        ParseFile(filePath);
    }
}

bool ObjRefsView::ShowRefTree(RefTree::Node::Ptr obj)
{
    if (!obj)
        return false;
    
    
    std::string searchFilter(refTreeSearchFilter);
    bool hasMatchingChild = false;
    
    if (!obj->children.empty())
    {
        for (const auto& child:obj->children)
        {
            
            std::string path = child->obj->classType + " " + child->obj->path;
            
            {

                {
                    hasMatchingChild = true;
                    
                    if (obj->bcycle)
                    {
                        std::string cycleLabel =  "[CYCLE]" + path;
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f)); // 红色标记循环
                        ImGui::TreeNodeEx(cycleLabel.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                        ImGui::PopStyleColor();
                    }
                    else
                    {

                        int flags = 0;
                        if (child->children.empty())
                        {
                            flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        }
                        ImGui::PushID(child.get());
                        auto open = ImGui::TreeNodeEx(path.c_str(), flags);
                        ImGui::PopID();
                        
                        if (open && flags == 0)
                        {

                            ShowRefTree(child);
                            
                            ImGui::TreePop();
                        }
         
                    }
                }
            }

        }
    }
    
    return hasMatchingChild;
}

void ObjRefsView::Show()
{
    // 创建窗口
    if (!windowOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
        windowOpen = true;
    }
    
    if (!ImGui::Begin("ObjRefsView", &windowOpen))
    {
        ImGui::End();
        return;
    }
    
    // 打开文件按钮
    if (ImGui::Button("Open File"))
    {
        OpenFile();
    }
    
    if (!currentFilePath.empty())
    {
        ImGui::SameLine();
        ImGui::Text("File: %s", currentFilePath.c_str());
    }
    
    ImGui::Separator();
    
    // 如果已加载数据，显示三列布局
    if (!typeToObjects.empty())
    {
        // 计算类型统计（按数量降序）
        std::vector<std::pair<std::string, size_t>> typeCounts;
        for (const auto& pair : typeToObjects)
        {
            typeCounts.push_back({pair.first, pair.second.size()});
        }
        std::sort(std::execution::par, typeCounts.begin(), typeCounts.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        


        // 三列布局，使用表格实现可调整列宽
        ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoBordersInBody;
        if (ImGui::BeginTable("##MainContent", 3, tableFlags))
        {
            // 设置列宽比例
            ImGui::TableSetupColumn("Types", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn("Objects", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Reference Tree", ImGuiTableColumnFlags_WidthStretch, 0.40f);
            ImGui::TableHeadersRow();
            
            ImGui::TableNextRow();
            
            // 第一列：类型列表
            ImGui::TableSetColumnIndex(0);
            ImGui::BeginChild("##TypeList", ImVec2(0, 0), true);
            

            // 计算总对象数量
            ImGui::Text("Total Objects: %zu", pathToObject.size());
            // 搜索框
            ImGui::Text("Search Types:");
            ImGui::InputText("##TypeSearch", typeSearchFilter, sizeof(typeSearchFilter));
            ImGui::Checkbox("Ignore Case##Type", &ignoreCase);
            ImGui::Separator();
            
            // 过滤并显示类型列表
            std::string typeFilter(typeSearchFilter);
            for (const auto& typeCount : typeCounts)
            {
                std::string typeName = typeCount.first.empty() ? "(empty)" : typeCount.first;
                if (MatchString(typeName, typeFilter, ignoreCase))
                {
                    std::string label = std::format("{} ({})", typeName, typeCount.second);
                    if (ImGui::Selectable(label.c_str(), selectedType == typeCount.first))
                    {
                        selectedType = typeCount.first;
                        selectedObject.reset(); // 清除选中的对象
                    }
                }
            }
            
            ImGui::EndChild();
            
            // 第二列：对象列表
            ImGui::TableSetColumnIndex(1);
            ImGui::BeginChild("##ObjectList", ImVec2(0, 0), true);
            
            // 搜索框
            ImGui::Text("Search Objects:");
            ImGui::InputText("##ObjectSearch", objectSearchFilter, sizeof(objectSearchFilter));
            ImGui::Checkbox("Ignore Case##Object", &ignoreCase);
            
   
            
            ImGui::Separator();
            
            if (typeToObjects.find(selectedType) != typeToObjects.end())
            {
                const auto& objects = typeToObjects[selectedType];
                std::string objectFilter(objectSearchFilter);
                
                // 创建排序后的对象列表（按名称排序）
                std::vector<UObjectRef::Ptr> sortedObjects(objects.begin(), objects.end());
                std::sort(sortedObjects.begin(), sortedObjects.end(),
                    [](const UObjectRef::Ptr& a, const UObjectRef::Ptr& b) {
                        return a->path < b->path;
                    });
                
                bool hasMatch = false;
                size_t matchCount = 0;
                for (const auto& obj : sortedObjects)
                {
                    std::string displayName = obj->path;
                    if (MatchString(displayName, objectFilter, ignoreCase))
                    {
                        hasMatch = true;
                        matchCount++;
                        if (ImGui::Selectable(displayName.c_str(), selectedObject == obj))
                        {
                            selectedObject = obj;

                            CacheSortedParents();
                        }
                    }
                }
                
                // 显示当前类型的对象数量和匹配数量
                if (!objectFilter.empty())
                {
                    ImGui::Text("Showing: %zu / %zu", matchCount, objects.size());
                }
                else
                {
                    ImGui::Text("Total: %zu", objects.size());
                }
                
                if (!hasMatch && !objectFilter.empty())
                {
                    ImGui::Text("No objects match the search filter");
                }
            }
            else if (!selectedType.empty())
            {
                ImGui::Text("No objects found for selected type");
            }
            
            ImGui::EndChild();
            
            // 第三列：引用关系树
            ImGui::TableSetColumnIndex(2);
            ImGui::BeginChild("##RefTree", ImVec2(0, 0), true);
            
            // 搜索框
            ImGui::Text("Search References:");
            ImGui::InputText("##RefTreeSearch", refTreeSearchFilter, sizeof(refTreeSearchFilter));
            ImGui::Checkbox("Ignore Case##RefTree", &ignoreCase);
            ImGui::Separator();
            
            if (selectedObject)
            {
                ImGui::BeginChild("##TreeContent", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                // 先显示选中的对象
                ImGui::Text("%s", selectedObject->path.c_str());
                // 然后显示引用关系树
                std::string refFilter(refTreeSearchFilter);
                bool showAll = refFilter.empty();
                ShowRefTree(reftree.root);
                ImGui::EndChild();
            }
            else
            {
                ImGui::Text("Select an object to view references");
            }
            
            ImGui::EndChild();
            
            ImGui::EndTable();
        }
    }
    else
    {
        ImGui::Text("Please open a file to view object references");
    }
    
    ImGui::End();
}


void ObjRefsView::CacheSortedParents()
{
    std::unordered_set<UObjectRef*> visited;
    auto root = RefTree::Node::Create(selectedObject,0);

    auto make = [&](RefTree::Node* node, auto& func)->int
    {
        int depth = 0;

        if (visited.contains(node->obj.get()))
        {
            node->bcycle = true;
            return 1;
        }
        visited.insert(node->obj.get());
        for (auto& obj : node->obj->parents)
        {
            auto cur = RefTree::Node::Create(obj, node);

            depth = std::max(depth, func(cur.get(), func));

            node->children.push_back(cur);
        }

        std::sort(node->children.begin(), node->children.end(), []( const auto& a,const auto& b){return a->depth > b->depth;});
        node->depth = depth + 1;

        return depth + 1;
    };
    make(root.get(),make);

    reftree.root = root;

}

