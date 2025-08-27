#include "MemViewer.h"
#include "imgui.h"
#include "Utils.h"
#include <format>
#include <filesystem>
#include <fstream>
#include "Concurrency.h"
#include <Windows.h>
#include "Component.h"
#include "FileBrowser.h"
#include <sstream>
static std::string list_files_api = "curl http://9.134.132.170:8080/list_files";
static std::string download_file_api = "curl http://9.134.132.170:8080/files/";
static std::string list_spans_api = "curl http://9.134.132.170:8080/list_spans";
static std::string download_span_api = "curl http://9.134.132.170:8080/spans/";

static std::string list_api = list_files_api;
static std::string download_api = download_file_api;

static std::string getDay(const std::string& date)
{

	auto weeksBetween = [](const std::tm & date) {
		// 获取当前时间
		std::time_t now = std::time(nullptr);
		std::tm now_tm;
		localtime_s(&now_tm, &now);

		//// 将std::tm转换为std::time_t
		//std::time_t date_time = std::mktime(const_cast<std::tm*>(&date));
		//std::time_t now_time = std::mktime(&now_tm);

		//// 计算两个时间点之间的差值（秒）
		//double difference = std::difftime(now_time, date_time);

		//// 将秒转换为天数
		//int days = difference / (60 * 60 * 24);
		
		// 获取年份和自然周编号
		char buffer1[10], buffer2[10];
		strftime(buffer1, sizeof(buffer1), "%Y%W", &date); // %Y: 年份, %W: 自然周编号（周一为一周的开始）
		strftime(buffer2, sizeof(buffer2), "%Y%W", &now_tm);

		// 转换为整数（年份 * 100 + 周数）
		int week1 = std::stoi(buffer1);
		int week2 = std::stoi(buffer2);

		return std::abs(week1 - week2);

		
	};

	std::istringstream date_stream(date);
	int year, month, day;
	char delimiter;

	date_stream >> year >> delimiter >> month >> delimiter >> day;

	std::tm time_in = {};
	time_in.tm_year = year - 1900; // tm_year is years since 1900
	time_in.tm_mon = month - 1; // tm_mon is 0-11
	time_in.tm_mday = day; // tm_mday is 1-31

	std::time_t time_temp = std::mktime(&time_in);
	//const std::tm* time_out = std::localtime(&time_temp);
	std::tm time_out;
	localtime_s(&time_out, &time_temp);
	
	int weekday = time_out.tm_wday; // 0 = Sunday, 1 = Monday, ..., 6 = Saturday

	int num_week = weeksBetween(time_out) ;
	//int num_week = num_days / 7;
	std::string output ;

	if (num_week <= 2)
	{
		for (int i = 0; i < num_week; ++i)
		{
			output += GBKToUTF8("上");
		}
		const char* days[] = { "周日", "周一", "周二", "周三", "周四", "周五", "周六" };
		output += GBKToUTF8(days[weekday]);
		output = std::format("({})", output);
	}

	return output;
}

FileBrowserView::FileBrowserView()
{
	file_list = split_string(run_cmd(list_api), "|");
	std::stable_sort(file_list.begin(), file_list.end(), std::greater<std::string>());
	is_running = std::make_shared<bool>();
	*is_running = false;
}

void FileBrowserView::ShowFileBrowser(std::function<void(std::string, int)>&& callback)
{
	if (*is_running)
	{
		return;
	}


	const ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

	static int selected_type = 0;


	if (ImGui::Button("refresh"))
	{
		file_list = split_string(run_cmd(list_api), "|");
		std::stable_sort(file_list.begin(), file_list.end(), std::greater<std::string>());
	}
	ImGui::SameLine();
	if (ImGui::Button("Open") && selected_file >= 0 && selected_file < file_list.size())
	{
		std::string local_file_path;
		if (file_list[selected_file].find(".zip") != std::string::npos)
		{
			local_file_path = file_list[selected_file].substr(0, file_list[selected_file].size() - 4);
		}

		if (!std::filesystem::exists(local_file_path))
		{
			local_file_path = std::format("./caches/{0}", file_list[selected_file]);
		}
		if (!std::filesystem::exists(local_file_path))
		{
			*is_running = true;
			AsyncTask(std::format("downloading file {0}", file_list[selected_file]), 
				[type = selected_type,is_running = is_running,callback,local_file_path, selected_file_path = file_list[selected_file]]()->bool {
					auto content = run_cmd(std::format("{0}{1}", download_api, selected_file_path));
					if (content.empty())
						return false;
					std::filesystem::create_directories("./caches/");
					std::fstream file = std::fstream(local_file_path, std::ios::binary | std::ios::out);
					file.write(content.c_str(), content.length());
					return true;
				},
				[type = selected_type,is_running = is_running,callback,local_file_path, filename = file_list[selected_file]](bool ret) {
					if (ret)
					{
						callback(local_file_path, type);
					}
					else
					{
						Error("failed to download file {0}", filename);
					}

					*is_running = false;
				}
			);
		}
		else
		{
			callback(local_file_path, selected_type);
		}

	}


	ImGui::SameLine();

	const char* list_type[] = { "trace", "span"};
	ImGui::SetNextItemWidth(250);

	if (ImGui::Combo("##views", &selected_type, list_type, 2))
	{
		if (selected_type == 0)
		{
			list_api = list_files_api;
			download_api = download_file_api;
		}
		else
		{
			list_api = list_spans_api;
			download_api = download_span_api;
		}
		file_list = split_string(run_cmd(list_api), "|");
		std::stable_sort(file_list.begin(), file_list.end(), std::greater<std::string>());
	}



	const int show_days = 14;
	static FilterComp filter;
	filter.Show();
	std::string filter_keyword = filter.GetString();
	filter_keyword = to_lower(filter_keyword);
	if (filter_keyword.empty())
	{
		std::string_view cur_day;
		uint32_t id = 1;
		bool open = false;
		int idx = 0;
		for (auto& file : file_list)
		{
			if (cur_day != std::string_view(file.data(), 10))
			{
				if (open)
					ImGui::TreePop();
					
				if (id < show_days)
					ImGui::SetNextItemOpen(true, ImGuiCond_Once);
				cur_day = std::string_view(file.data(), 10);
				open = ImGui::TreeNodeEx((void*)id++, ImGuiTreeNodeFlags_SpanAllColumns,std::format("{1} {0}",getDay(file), file.substr(0, 10)).c_str() );
			}

			if (open)
			{
				auto display_str = GBKToUTF8(file);
				if (ImGui::Selectable(display_str.data(), selected_file == idx))
				{
					selected_file = idx;
				}
			}
			idx++;
		
		}
		if (open)
			ImGui::TreePop();
	}
	else
	{
		std::vector<std::string> keywords = split_string(filter_keyword, " ");
		if (ImGui::BeginTable("filelist", 1, tbl_flags))
		{
			ImGui::TableSetupColumn("File Name", ImGuiTableColumnFlags_NoHide);

			int idx = 0;
			for (auto& f : file_list)
			{
				auto name = to_lower(f);
				auto matched = [&]()
				{
					for (auto& key : keywords)
					{
						if (name.find(key) == std::string::npos)
						{
							return false;
						}
					}


					return true;
				}();

				if (!matched)
				{
					idx++;
					continue;
				}
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				if (ImGui::Selectable(std::format("{}{}",getDay(f), GBKToUTF8(f)).c_str(), selected_file == idx, ImGuiSelectableFlags_DontClosePopups))
				{
					selected_file = idx;
				}
				idx++;
			}

			ImGui::EndTable();
		}
	}


}