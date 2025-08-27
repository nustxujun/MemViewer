#pragma once

#include "View.h"
#include "Component.h"

//class SnapshotView: public View
//{
//public:
//	using View::View;
//
//	virtual void InitializeImpl() override;
//	virtual void UpdateImpl()  override;
//	virtual void ShowImpl()  override;
//
//
//    void TakeSnapshot(const struct SnapshotInfos& infos);
//    void MakeView();
//    void MakeStack();
//    void MakeCalltree();
//
//    struct SnapshotInfo
//    {
//        std::string name;
//        struct Node* root;
//        int count;
//    };
//
//    std::vector<SnapshotInfo> snapshots;
//    CategoryComp Comp;
//
//    struct Node* diff = 0;
//
//    int selected_first = -1;
//    int selected_second = -1;
//    struct Node* selected_node = 0;
//};
