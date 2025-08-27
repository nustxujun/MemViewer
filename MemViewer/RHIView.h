#pragma once


#include "View.h"
#include "TraceParser.h"
#include "Component.h"
#include <vector>

class RHIView : public View
{
public:
	using View::View;

	virtual void InitializeImpl() override;
	virtual void UpdateImpl()  override;
	virtual void ShowImpl()  override;

	void MakeInfos();

	struct RHITypeInfo
	{
		std::string name;
		int32_t count = 0;
		int64_t size = 0;

		std::vector<RhiInfo> rhis;
	};

	struct SubTypeInfo
	{
		std::string name;
		int32_t count = 0;
		int64_t size = 0;

		std::vector<RHITypeInfo> rhis;
	};

	struct TypeInfo
	{
		int type = 0;
		std::string name;
		int32_t count = 0;
		int64_t size = 0;

		std::vector<SubTypeInfo> infos;
	};



	std::vector<TypeInfo> types;

	FilterComp name_filter;
	CategoryComp categories;

	TableList tables;
	RhiInfo* selected_rhi_res = 0;

	int64_t show_size = 0;

	int selected_type = 0;
	int selected_subtype = 0;
	int selected_rhi = 0;
	int selected_texture_type = 0;
	int selected_texture_format = 0;
	int selected_rhi_type = 0;

	bool inverse_filter = 0;
};