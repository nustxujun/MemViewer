#pragma once


#include "View.h"
#include "TraceParser.h"
#include "Component.h"
#include <vector>
#include <map>

class CustomObj : public View
{
public:
	using View::View;

	virtual void InitializeImpl() override;
	virtual void UpdateImpl()  override;
	virtual void ShowImpl()  override;



	FilterComp name_filter;
	CategoryComp categories;

	std::map<int, std::vector<CustomData::Ptr>> customs;
};