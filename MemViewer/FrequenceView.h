#pragma once


#include "View.h"
#include <vector>
#include <unordered_map>

class FrequenceView : public View
{
public:
	using View::View;
private:
	virtual void InitializeImpl() override;
	virtual void UpdateImpl()  override;
	virtual void ShowImpl()  override;

	void UpdateTimeline();
	void MakeOrderedInfos();
	void MakeStack();

	struct FreqInfo 
	{
		uint32_t count = 0;
		uint32_t node_index = 0;
		bool matched = true;
	};

	std::vector<FreqInfo> freq_infos;
	std::unordered_map<uint32_t, std::vector<float>> timelines;
	int scaling = 1;
	int selected_item = 0;
};
