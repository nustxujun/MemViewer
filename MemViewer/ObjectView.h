#pragma once


#include "View.h"
#include <vector>


class ObjectView : public View
{
public:
	using View::View;

	virtual void InitializeImpl() override;
	virtual void UpdateImpl()  override;
	virtual void ShowImpl()  override;


	struct ObjectNode
	{
		using Ptr = std::shared_ptr<ObjectNode>;
		std::string name;

		std::vector<Ptr> children;
		std::vector<const struct ObjectInfo*> objs;

		int64_t total_add = 0;
		int64_t total_delete = 0;

		int matching = 2;
	};


	ObjectNode::Ptr root;
	int selected_show_type = 0;
	const struct ObjectInfo* selected = 0;
	char filter[1204] = {};

};