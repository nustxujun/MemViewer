//#pragma once
//
//
//#include "View.h"
//#include <vector>
//#include <unordered_map>
//
//class BlockView : public View
//{
//public:
//	using View::View;
//private:
//	virtual void InitializeImpl() override;
//	virtual void UpdateImpl()  override;
//	virtual void ShowImpl()  override;
//
//	void MakeBlocks();
//	void MakeSymbols();
//	void MakeStacks();
//	void UpdateTimeline();
//
//	struct BlockInfo
//	{
//		uint32_t total_count = 0;
//		uint32_t total_size = 0;
//		int block_size = 0;
//		int index = 0;
//	};
//
//	struct AllocInfo
//	{
//		uint32_t node_index = 0;
//		uint32_t size = 0;
//		uint32_t count = 0;
//		uint32_t block_size = 0;
//	};
//
//	std::vector<BlockInfo> infos;
//	std::vector<std::vector<AllocInfo>> allocs;
//	std::vector<std::vector<float>> timeline_data;
//	int selected_block = 0;
//	int selected_symbol = 0;
//	int scaling = 1;
//};
