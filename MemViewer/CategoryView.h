#pragma once

#include "View.h"
#include <string>
#include <vector>
#include <array>

#include "TraceParser.h"

class CategoryView : public View
{
public:
	//using View::View;
	CategoryView(std::string n, class TimelineView* tl = nullptr) :View(std::move(n)),timeline_view(tl)
	{
		temp.fill(0);
	}
private:
	virtual void InitializeImpl() override;
	virtual void UpdateImpl()  override;
	virtual void ShowImpl()  override;

	struct Category
	{
		std::string name;
		std::vector<std::string> filters;
		int64_t total_size = 0;
		int index = 0;
	};
	struct AllocInfo
	{
		NodeRef node_index ;
		NodeRef matched_node_index;
		int64_t count = 0;
		int64_t size = 0;
	};


	static uint32_t MatchCategory(const std::string& str, const std::vector<CategoryView::Category>& cats);
	uint32_t findCategory();
	uint32_t GetUntagged(const std::vector<Category>& cats);
	void UpdateSubCategory();
	void MakeCategory();
	void MakeSubCategory();
	void MakeSymbols();
	void MakeStack();
	void UpdateTimeline(std::vector<float> timeline);

	class TimelineView* timeline_view;

	std::vector<Category> categories;
	std::vector< std::vector<Category>> sub_categories;
	std::vector<std::vector<AllocInfo>> alloc_infos;
	std::vector< std::vector<AllocInfo>> sub_alloc_infos;
	//std::vector<std::vector<float>> timeline_datas;

	std::vector<std::string> symbol_filters;
	uint32_t selected_category = 0;
	uint32_t selected_sub_category = 0;
	uint32_t selected_symbol = 0;
	int show_level = 0;
	int scaling = 1;
	int selected_config_file = 0;
	std::array<char, 1024> temp ;


	//int selected_config_file = 0;
};