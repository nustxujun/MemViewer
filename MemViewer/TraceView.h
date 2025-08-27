#pragma once

#include <vector>
#include <string>

#include "FileBrowser.h"
#include "View.h"
#include <format>

class TabView 
{
public : 
	using Ptr = std::shared_ptr<TabView>;
	std::string num_str;

	TabView()
	{
		static int unique_win_id = 1;
		num_str = std::format(" #{}", unique_win_id++);
	}
	virtual ~TabView() = default;

	virtual void Show(int group, int index) = 0;

	virtual std::string getName() = 0;
	virtual std::string getLongName() = 0;

	virtual void discard() = 0;
	virtual std::shared_ptr<TraceInstanceBasic> getTrace() {return {};};
};

class StandaloneView: public TabView
{
	std::string name;
	View::Ptr view;
	uint8_t* state;
public :
	
	StandaloneView(std::string n, View::Ptr v, uint8_t* s):name(std::move(n)), view(v), state(s)
	{
	}

	~StandaloneView()
	{
		*state = 0;
	}

	virtual void Show(int group, int index) override;
	virtual std::string getName() override
	{
		return name;
	}
	virtual std::string getLongName() override
	{
		return name;
	}

	virtual void discard()
	{
		view->syncTask(true);
	}
};

class SpanView :public TabView
{
	std::string name;
	std::string long_name;
	std::vector<View::Ptr> views;
	std::vector<uint8_t> standalones;
	class TraceView* trace_view;
	std::shared_ptr<TraceInstanceBasic> trace_file;
public:

	SpanView(std::string n, std::string ln, TraceView* view, std::shared_ptr<TraceInstanceBasic> data) ;

	~SpanView()
	{
	}

	virtual void Show(int group, int index) override;
	virtual std::string getName() override
	{
		return name;
	}
	virtual std::string getLongName() override
	{
		return long_name;
	}
	virtual std::shared_ptr<TraceInstanceBasic> getTrace() { return trace_file; };
	virtual void discard()
	{
		for(auto& v:views)
		{
			v->syncTask(true);
		}
	}
	void upload(std::string name);
};



class TraceInstance;
class TraceView
{

	struct Trace : public TabView, public std::enable_shared_from_this<Trace>
	{
		using Ptr = std::shared_ptr<Trace>;
		std::string path;
		std::shared_ptr<TraceInstanceBasic> trace_file;

		FileBrowserView file_browser;

		View::Ptr timeline;
		std::vector<View::Ptr> views;
		std::vector<uint8_t> standalones;
		TraceView* trace_view;
		int index = 0;
		int span_index = 0;
		Trace(TraceView* v);
		~Trace()
		{
		}
		void Show(int group, int index);
		virtual std::string getName() override;
		virtual std::string getLongName() override;
		virtual std::shared_ptr<TraceInstanceBasic> getTrace() { return trace_file; };
		virtual void discard()
		{
			timeline->syncTask(true);
			for (auto& v : views)
			{
				v->syncTask(true);
			}
		}
	};
	struct SplitView
	{
		std::vector<TabView::Ptr> traces;
	};
public :
	void Show();

	TraceView();
	void AddNewTrace(int split);
	void AddTab(int grp, TabView::Ptr view);
	void Move(int src_grp, int src_idx, int dst_grp);
	void MoveToNewSplit(int src_grp, int src_idx);
	void Compare(int src_grp, int src_idx);
	void Span(int group, int dst_idx);
private:
	std::vector<std::function<void()>> move_functions;


	std::vector< SplitView> splits;
	bool popup_comparation = false;
	int CmpGrp = 0;
	int CmpIdx = 0;
};