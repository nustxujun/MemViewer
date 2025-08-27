#pragma once

#include <string>
#include <ranges>
#include <algorithm>
#include <format>
#include <chrono>
#include <iostream>
#include "imgui/imgui.h"

#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))
#define ALIGN(x,a)              __ALIGN_MASK(x,((decltype(x))a)-1)

#define Log(...) {std::cout <<"INFO:"<<std::format(__VA_ARGS__) << std::endl;}
#define Warning( ...) {std::cout << "\n\nWARNING: " <<std::format(__VA_ARGS__) << std::endl;}
#define Error( ...) {Warning(__VA_ARGS__);*((int*)0) = 0;}
#define Assert(cond, ...) {if (!(cond)) {Error(__VA_ARGS__);}}

static std::string to_lower(const std::string& val)
{
	std::string ret;
	ret.resize(val.size());
	std::transform(val.begin(), val.end(), ret.begin(), [](unsigned char c) { return std::tolower(c); });
	return ret;
}

static std::vector<std::string> split_string(const std::string& in, const std::string& delim)
{
	std::vector<std::string> ret;
	for (const auto& word : std::views::split(std::string(in), delim)) {
		ret.push_back(std::string(word.begin(), word.end()));
	}
	return ret;
}

static std::string size_tostring(int64_t size)
{
	const float k = 1024;
	const float m = 1024 * k;

	if (std::abs(size) >= m)
	{
		return std::format("{0:.2f} M", size / m);
	}
	else if (std::abs(size) >= k)
	{
		return std::format("{0:.2f} K", size / k);
	}
	else
	{
		return std::format("{0}", size);
	}

}

static std::string run_cmd(const std::string& cmd)
{
	char   psBuffer[1024];
	FILE* pPipe;
	std::cout << cmd << std::endl;
	if ((pPipe = _popen(cmd.c_str(), "rb")) == NULL)
		return {};

	std::string content;
	size_t count;
	while (count = fread(psBuffer, 1, sizeof(psBuffer), pPipe)) {
		content += std::string(psBuffer, count);
	}


	if (feof(pPipe))
		_pclose(pPipe);

	return content;

}


struct Counter
{
	using tm = std::chrono::high_resolution_clock;
	std::string name;
	tm::time_point time;
	Counter(const std::string& str):name(str)
	{
		time = tm::now();
	}

	~Counter()
	{
		auto diff = (tm::now() - time).count() / 1000000000.0f;
		std::cout << std::format("{0} cost {1} s\n", name, diff);
	}
};

extern std::string get_or_create_default_file(int id, const char* path);
extern bool Spinner(const char* label, float radius, int thickness, const ImU32& color);
//extern struct Node* make_current_snapshot();
//
//
//struct SnapshotInfos
//{
//	std::string name;
//	struct Node* root = 0;
//	int count = 0;
//};
//
//extern std::string make_snapshot_to_string(SnapshotInfos infos);
//extern SnapshotInfos make_snapshot_from_string(std::string content);


extern std::string GBKToUTF8(const std::string& gbk_str);
extern std::string UTF8ToGBK(const std::string& gbk_str);