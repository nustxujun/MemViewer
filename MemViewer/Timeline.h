#pragma once


#include "View.h"
#include "imgui/imgui.h"

class TimelineView : public View
{
public:
	using View::View;
	void InitializeImpl() ;
	void UpdateImpl() {}
	void ShowImpl() ;

	void notifySelectedRange(std::function<void(uint32_t, uint32_t)>&& f){ select_range_cb = std::move(f); }
	void SetCustomData(std::vector<float> datas);

private:
	ImVec2 scrolling = {0.0f, 0.0f};
	std::vector<float> frames;
	std::vector<float> custom_datas;
	std::vector<float> custom_datas2;

	std::function<void(uint32_t, uint32_t)> select_range_cb;

	float max_size = 2000;
	uint32_t base_frame = 0;
	uint32_t dragging_start = 0;
	uint32_t dragging_end = 0;
	int avg_filter = 1;
	float total_scaling = 1.0f;

	ImVector<ImVec2> points;
	bool opt_enable_context_menu = true;
	int freq = 1;
	bool is_scrolling = false;
	bool is_dragging = false;

};