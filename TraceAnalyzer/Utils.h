#pragma once

#include <string>
#include <ranges>
#include <algorithm>
#include <format>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <stdexcept>

#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))
#define ALIGN(x,a)              __ALIGN_MASK(x,((decltype(x))a)-1)

using PrintLogFunc = void(*)(const std::string&);
extern PrintLogFunc g_print_log_func;
inline void print_log(const std::string& content) { if (g_print_log_func) g_print_log_func(content); }

#define Log(...) { print_log("INFO: " + std::format(__VA_ARGS__)); }
#define Warning(...) { print_log("\nWARNING: " + std::format(__VA_ARGS__)); }
#define Error(...) { auto _err_msg_ = std::format(__VA_ARGS__); Warning("{}", _err_msg_); throw std::runtime_error(_err_msg_); }
#define Assert(cond, ...) { if (!(cond)) { Error(__VA_ARGS__); } }

inline std::string to_lower(const std::string& val)
{
	std::string ret;
	ret.resize(val.size());
	std::transform(val.begin(), val.end(), ret.begin(), [](unsigned char c) { return std::tolower(c); });
	return ret;
}

inline std::vector<std::string> split_string(const std::string& in, const std::string& delim)
{
	std::vector<std::string> ret;
	for (const auto& word : std::views::split(std::string(in), delim)) {
		ret.push_back(std::string(word.begin(), word.end()));
	}
	return ret;
}

inline std::string size_tostring(int64_t size)
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

struct Counter
{
	using tm = std::chrono::high_resolution_clock;
	std::string name;
	tm::time_point time;
	Counter(const std::string& str) : name(str)
	{
		time = tm::now();
	}

	~Counter()
	{
		auto diff = (tm::now() - time).count() / 1000000000.0f;
		Log("{0} cost {1} s\n", name, diff);
	}
};
