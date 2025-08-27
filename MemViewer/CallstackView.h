#pragma once

#include "View.h"

#include <vector>
#include <string>
#include "TraceInstance.h"

class CallstackView : public View
{
public:
	using View::View;
private:
	virtual void InitializeImpl() override;
	virtual void UpdateImpl()  override;
	virtual void ShowImpl()  override;

	void CalSize(struct Node* node, bool ignore_node = true);
	void MakeTreeView(struct Node* node);
	void FilterByString(const char* infilters);

	Calltree::Ptr calltree;
	std::vector<std::string> filters;
	bool case_sensitive = false;
	char input_buffer[1024] = {};

};
