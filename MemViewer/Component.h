#pragma once

#include <vector>
#include <functional>
#include <string>

struct FilterComp
{
	bool Show(int flag = 0);
	bool filter(const std::string& cnt)
	{
		if (str[0] != 0)
			return cnt.find(str) != std::string::npos;
		else
			return true;
	}
	const char* GetString(){return str;};
	char str[256] = {};
};



struct TableComp
{
	struct Table
	{
		std::string name;
		int flags;
		int width;
		std::vector<std::string> col_names;
		std::vector<int> col_flags;
		std::vector<int> col_widths;
		std::function<bool(int, std::vector<std::string>&)> item_callback;
		std::function<void(int)> on_select;
		std::function<void(int, int)> on_sort;
		std::function<void()> header;
		std::string selected_name;
		int selected = 0;
	};
	void Init(std::vector<Table> tbls);
	void Show();

	std::vector<Table> tables;
	
};

struct CategoryComp
{
	struct Data
	{
		std::string name;
		int64_t size;
		int64_t count;
		void* userdata;
	};

	TableComp table;


	struct SubCategory
	{
		SubCategory(const std::string& n) :name(n)
		{

		}

		std::string name;
		std::vector<std::string> keys;
		std::vector<Data> datas;
		int64_t size = 0;
		int64_t count = 0;
	};

	struct Category
	{
		Category(const std::string& n) :name(n)
		{}

		std::string name;
		std::vector<std::string> keys;
		std::vector<SubCategory> subs;
		int64_t size = 0;
		int64_t count = 0;
	};

	std::vector<Category>& Init(const std::string& config_file);
	std::vector<Category>& Init(const std::vector<std::string>& cate);
	void LoadFile();
	void InitTable(std::function<void(void*)>&& on_selected_item, std::function<void(const std::string&, const std::string&)>&& on_show, std::function<bool(bool, void*, const std::string& key)>&& filter);
	void Update( std::function<void(void*)>&& on_selected_item, std::function<void(const std::string&, const std::string&)>&& on_show ,std::function<bool(bool, void*, const std::string& key)>&& filter);
	void Show();

	FilterComp filter_comp;
	uint32_t show_size;
	uint32_t show_count;
	std::vector<Category> groups;
	std::string config_file;
	int selected_category = 0;
	int selected_subcategory = 0;
	int selected_item = 0;
};


struct TableList
{
	struct TableDescriptor
	{
		struct Header
		{
			std::string name;
			int flag = 0;
			int width = 0;
		};
		using string_line = std::vector<std::string>;
		using item = std::pair<int, string_line>;

		TableDescriptor(std::function<std::vector<Header>()>&& h, std::function<std::vector<item>()>&& i, std::function<void(int, int)>&& sort, std::function<void(int)>&& select, std::function<void(TableDescriptor&)> p_s = {}) : headers(std::move(h)), items(std::move(i)), on_sort(std::move(sort)), on_selected(std::move(select)), pre_show(std::move(p_s))
		{

		}

		std::function<std::vector<Header>()> headers;
		std::function<std::vector<item>()> items;
		std::function<void(int, int)> on_sort;
		std::function<void(int)> on_selected;
		std::function<void(TableDescriptor&)> pre_show;

		std::vector<Header> header_caches;
		std::vector<item> item_caches;

		int selected = 0;

		void Refresh()
		{
			header_caches = headers();
			item_caches = items();
			on_selected(selected);
		}
	}; 

	void Init(const std::vector<TableDescriptor>& tbls);
	void Show();

	std::vector<TableDescriptor> tables;

	void Refresh()
	{
		is_need_clear = true;
		for (auto& tbl : tables)
		{
			tbl.Refresh();
		}
	}
	bool is_need_clear = false;
};

struct CategoryParser
{
	struct SubCategory
	{
		SubCategory(const std::string& n) :name(n)
		{

		}

		std::string name;
		std::vector<std::string> keys;
	};

	struct Category
	{
		Category(const std::string& n) :name(n)
		{
		}

		std::string name;
		std::vector<std::string> keys;
		std::vector<SubCategory> subs;
	};


	std::vector<Category> operator()(const std::string& file_path);
};


struct ModalWindow
{
	static void OpenModalWindow(std::string val, std::function<void()>&& win);
	static void ProcessModalWindow();
};


struct TimelineComp
{
	int Count = 0;


	struct TimelineData
	{
		std::vector<float> datas;
		int type = 0;
		int order = 0;
		int color = 0;
	};

	std::vector<TimelineData> datas;

	void Show();
	void setDatas(int index, TimelineData datas);
};