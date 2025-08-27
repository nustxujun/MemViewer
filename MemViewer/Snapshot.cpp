//#include "Snapshot.h"
//#include "resource.h"
//#include "Utils.h"
//#include "imgui.h"
//#include "TraceParser.h"
//#include "Concurrency.h"
//#include <mutex>
//#include <atomic>
//
//
//void SnapshotView::InitializeImpl()
//{
//    
//}
//
//void SnapshotView::UpdateImpl()
//{
//
//}
//void SnapshotView::MakeView()
//{
//    auto& groups = Comp.Init(get_or_create_default_file(IDR_CATEGORY_CONFIG_INI, "category_config.ini"));
//
//    std::mutex mt;
//    CategoryComp::Category tail("Untagged");
//    tail.subs.emplace_back("untagged");
//
//    bool untagged_found = false;
//    for (auto& grp : groups)
//    {
//        if (grp.name == tail.name)
//        {
//            untagged_found = true;
//        }
//    }
//
//    if (!untagged_found)
//    {
//        groups.push_back(tail);
//    }
//
//    std::atomic_uint32_t total;
//
//    auto match = [&](Node* node,  const auto& match){
//        if (node->basic_size == 0)
//            return;
//        total += (uint32_t)node->basic_size;
//        auto cur = node;
//        while (node)
//        {
//            for (auto& grp : groups)
//            {
//                bool not_found = true;
//                for (auto& key : grp.keys)
//                {
//                    if (node->name.find(key) != std::string::npos)
//                    {
//                        not_found = false;
//                        break;
//                    }
//                }
//                if (not_found)
//                    continue;
//
//                node = cur;
//
//                while(node)
//                {
//                    for (auto& sub: grp.subs)
//                    {
//                        for (auto& key : sub.keys)
//                        {
//                            if (node->name.find(key) == std::string::npos)
//                                continue;
//
//                            node = cur;
//
//                            std::lock_guard lock(mt);
//                            grp.size += node->basic_size;
//                            grp.count += node->basic_count;
//                            sub.size += node->basic_size;
//                            sub.count += node->basic_count;
//                            sub.datas.push_back({node->name, node->basic_size, node->basic_count, node});
//                            return;
//                        }
//                    }
//
//                    node = node->parent;
//                }
//                node = cur;
//
//                std::lock_guard lock(mt);
//                grp.size += node->basic_size;
//                grp.count += node->basic_count;
//                auto& sub = grp.subs[grp.subs.size() - 1];
//                sub.size += node->basic_size;
//                sub.count += node->basic_count;
//                sub.datas.push_back({ node->name, node->basic_size, node->basic_count, node });
//
//                return;
//
//            }
//            node = node->parent;
//        }
//
//        node = cur;
//
//        std::lock_guard lock(mt);
//
//        groups.rbegin()->size += node->basic_size;
//        groups.rbegin()->count += node->basic_count;
//        auto& sub = groups.rbegin()->subs[groups.rbegin()->subs.size() - 1];
//        sub.size += node->basic_size;
//        sub.count += node->basic_count;
//        sub.datas.push_back({ node->name, node->basic_size, node->basic_count, node });
//    };
//
//
//    if (selected_second == -1)
//    {
//        auto& selected = snapshots[selected_first];
//
//        //for (int i = 0; i < selected.count; ++i)
//        //{
//        //    match(selected.root + i, match);
//        //}
//        ParallelTask([&](int idx)
//        {
//            match(selected.root + idx, match);
//        }, selected.count);
//
//        diff = CloneParsedNodes(selected.root,selected.count);
//
//        std::cout << (uint32_t)total.load() << std::endl;
//    }
//    else
//    {
//        const uint32_t total_node = snapshots[selected_first].count + snapshots[selected_second].count;
//        auto root = new Node[total_node];
//        root->name = "root";
//        uint32_t cur_count = 1;
//
//        auto create_node = [&](){
//            return root + (cur_count++);
//        };
//
//
//        auto create_tree = [&](Node* src, int32_t sign, auto& create)->Node*{
//
//            Node* n = create_node();
//            n->name = src->name;
//            n->basic_size = src->basic_size * sign;
//            n->basic_count = src->basic_count * sign;
//            
//            for (auto c : src->children)
//            {
//                auto c_n = create(c, sign, create);
//                c_n->parent = n;
//                n->children.push_back(c_n);
//            }
//            return n;
//        };
//
//        auto diff_tree = [&](Node* left, Node* right,Node* result, const auto& diff)->void{
//            
//            std::stable_sort(left->children.begin(), left->children.end(), [](auto& a, auto& b){
//                return a->name < b->name;
//            });
//            std::stable_sort(right->children.begin(), right->children.end(), [](auto& a, auto& b) {
//                return a->name < b->name;
//                });
//
//            auto rbegin = right->children.begin();
//            auto lbegin = left->children.begin();
//            for (; lbegin != left->children.end() && rbegin != right->children.end(); )
//            {
//                auto ln = *lbegin;
//                auto rn = *rbegin;
//                if (ln->name == rn->name)
//                {
//                    lbegin++;
//                    rbegin++;
//                    if (ln->size == rn->size)
//                    {
//                        continue;
//                    }
//                    else
//                    {
//                        Node* child = create_node();
//                        child->name = ln->name;
//                        child->parent = result;
//
//                        result->children.push_back(child);
//                        diff(ln, rn, child, diff);
//                    }
//                }
//                else
//                {
//                    if (ln->name < rn->name)
//                    {
//                        Node* child = create_tree(ln, -1, create_tree);
//                        child->name = ln->name;
//                        child->parent = result;
//                        child->basic_size = -ln->basic_size;
//                        child->basic_count = -ln->basic_count;
//                        result->children.push_back(child);
//                        lbegin++;
//                    }
//                    else
//                    {
//                        Node* child = create_tree(rn, 1, create_tree);
//                        child->name = rn->name;
//                        child->parent = result;
//                        child->basic_size = rn->basic_size;
//                        child->basic_count = rn->basic_count;
//                        result->children.push_back(child);
//                        rbegin++;
//                    }
//                }
//
//            }
//
//            for (auto i = lbegin; i != left->children.end(); ++i)
//            {
//                auto ln = *i;
//                Node* child = create_tree(ln,-1, create_tree);
//                child->name = ln->name;
//                child->parent = result;
//                child->basic_size = -ln->basic_size;
//                child->basic_count = -ln->basic_count;
//                result->children.push_back(child);
//            }
//
//            for (auto i = rbegin; i != right->children.end(); ++i)
//            {
//                auto rn = *i;
//                Node* child = create_tree(rn, 1, create_tree);
//                child->name = rn->name;
//                child->parent = result;
//                child->basic_size = rn->basic_size;
//                child->basic_count = rn->basic_count;
//                result->children.push_back(child);
//            }
//
//            result->basic_size = right->basic_size - left->basic_size;
//            result->basic_count = right->basic_count - left->basic_count;
//
//        };
//
//        diff_tree(snapshots[selected_first].root, snapshots[selected_second].root, root, diff_tree);
//
//
//        //for (int i = 0; i < cur_count; ++i)
//        //{
//        //    match(root+i, match);
//        //}
//
//        ParallelTask([&](int idx)
//            {
//                match(root + idx, match);
//            }, cur_count);
//
//        diff = root;
//
//
//        auto cal_size = [](Node* node, auto& cal)->void{
//            node->size =0;
//            node->count = 0;
//
//            for (auto c : node->children)
//            {
//                cal(c, cal);
//                node->size += c->size;
//                node->count += c->count;
//            }
//
//            std::stable_sort(node->children.begin(), node->children.end(), [](auto& a, auto&  b){
//                return a->size > b->size;
//            });
//
//            node->size += node->basic_size;
//            node->count += node->basic_count;
//
//        };
//
//        cal_size(root, cal_size);
//        std::cout << (uint32_t)total.load() << std::endl;
//    }
//
//    Comp.Update([&](void* data){
//        selected_node = (Node*)data;
//    },[&](const std::string& name, auto){
//    },[](bool passed_by_name, void* data, const std::string& key)->bool
//    {
//        auto node = (Node*) data;
//
//        while (node)
//        {
//            if (node->name.find(key) != std::string::npos)
//                return true;
//
//            node = node->parent;
//        }
//
//        return false;
//    });
//}
//
//
//void SnapshotView::MakeCalltree()
//{
//    if (!diff)
//        return;
//    auto stack = [](Node* node, const auto& stack)->void {
//
//        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns;
//
//        const bool is_leaf = node->children.size() == 0;
//
//        if (is_leaf)
//        {
//            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
//        }
//
//        if (ImGui::TreeNodeEx(node, flags, std::format("{0:16s} {1}", size_tostring(node->size), node->name).c_str()))
//        {
//            for (auto& c : node->children)
//            {
//                stack(c, stack);
//            }
//
//            if (!is_leaf)
//                ImGui::TreePop();
//        }
//
//    };
//    ImGui::BeginChild("calltree", { 0 ,0 }, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders);
//    stack(diff, stack);
//    ImGui::EndChild();
//}
//
//
//void SnapshotView::MakeStack()
//{
//    auto stack = [](Node* node, const auto& stack)->void{
//
//        ImGui::PushID(node);
//        ImGui::Selectable(node->name.c_str());
//        ImGui::PopID();
//
//
//        if (node->parent)
//            stack(node->parent, stack);
//
//    };
//    ImGui::BeginChild("callstack", { 0 ,0 }, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders);
//    stack(selected_node, stack);
//    ImGui::EndChild();
//}
//
//void SnapshotView::ShowImpl()
//{
//    static const ImGuiTableFlags tbl_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg ; 
//
//
//
//
//    ImGui::BeginChild("snapshotlist", {200 ,0}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders);
//
//    if (ImGui::BeginTable("snapshotlist", 1, tbl_flags))
//    {
//        ImGui::TableSetupColumn("Snapshot", ImGuiTableColumnFlags_NoHide);
//        ImGui::TableHeadersRow();
//
//        int idx = 0;
//        for (auto& sn : snapshots)
//        {
//            ImGui::TableNextRow();
//            ImGui::TableNextColumn();
//            auto selected = idx == selected_first || idx == selected_second;
//            ImGui::PushID(idx);
//            if (ImGui::Selectable(GBKToUTF8(sn.name).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
//            {
//                do
//                {
//                    if (diff)
//                    {
//                        delete[] diff;
//                        diff = 0;
//                    }
//                    if (selected_second != -1)
//                    {
//                        selected_second = -1;
//                        selected_first = -1;
//                    }
//                    else
//                    {
//                        if (selected_first == idx)
//                            break;
//                    }
//
//                    if (selected_first == -1)
//                    {
//                        selected_first = idx;
//                        MakeView();
//                    }
//                    else 
//                    {
//                        selected_second = idx;
//                        MakeView();
//                    }
//                }
//                while(0);
//            }
//            ImGui::PopID();
//            idx++;
//        }
//        ImGui::EndTable();
//    }
//
//    ImGui::EndChild();
//
//    ImGui::SameLine();
//    
//    ImGui::BeginChild("Views");
//    if (ImGui::BeginTabBar("Views"))
//    {
//        if (ImGui::BeginTabItem("calltree"))
//        {
//            MakeCalltree();
//            ImGui::EndTabItem();
//        }
//        if (ImGui::BeginTabItem("categories"))
//        {
//            Comp.Show();
//
//            ImGui::SameLine();
//            if (selected_node)
//            {
//                MakeStack();
//            }
//
//
//            ImGui::EndTabItem();
//        }
//        ImGui::EndTabBar();
//
//    }
//    ImGui::EndChild();
//}
//
//
//void SnapshotView::TakeSnapshot(const SnapshotInfos& infos)
//{
//    auto count = infos.count;
//    snapshots.push_back({ infos.name, infos.root, count});
//}
//
