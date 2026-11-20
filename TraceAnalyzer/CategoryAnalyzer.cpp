#include "CategoryAnalyzer.h"
#include "Concurrency.h"
#include "Utils.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <mutex>
#include <algorithm>

std::vector<CategoryConfig::Category>
CategoryAnalyzer::ParseConfig(const std::string& file_path)
{
	std::vector<CategoryConfig::Category> groups;

	if (!std::filesystem::exists(file_path))
		return groups;

	auto f = std::fstream(file_path, std::ios::in);
	char temp[1024 * 4] = {};
	while (!f.eof() && f.good())
	{
		f.getline(temp, 1024 * 4);
		std::regex pattern("([^=]+)=?([^=]*)");
		std::smatch results;
		std::string content = temp;
		const bool is_sub = temp[0] == '+';

		if (std::regex_search(content, results, pattern))
		{
			if (is_sub && !groups.empty())
			{
				auto& parent = groups.back();
				CategoryConfig::SubCategory sub;
				sub.name = results[1].str().substr(1);
				sub.keys = split_string(results[2], "|");
				parent.subs.push_back(std::move(sub));
			}
			else
			{
				CategoryConfig::Category cat;
				cat.name = results[1];
				cat.keys = split_string(results[2], "|");
				groups.push_back(std::move(cat));
			}
		}
	}

	if (groups.empty() || groups.back().name != "Untagged")
	{
		CategoryConfig::Category untagged;
		untagged.name = "Untagged";
		groups.push_back(std::move(untagged));
	}
	else
	{
		groups.back().keys.clear();
	}

	for (auto& grp : groups)
	{
		CategoryConfig::SubCategory untagged_sub;
		untagged_sub.name = "Untagged";
		grp.subs.push_back(std::move(untagged_sub));
	}

	return groups;
}

int CategoryAnalyzer::MatchCategory(const std::string& name,
                                    const std::vector<CategoryConfig::Category>& cats)
{
	int cat_idx = 0;
	for (auto& cat : cats)
	{
		for (auto& key : cat.keys)
		{
			if (name.find(key) != std::string::npos)
				return cat_idx;
		}
		cat_idx++;
	}
	return -1;
}

std::vector<CategoryMapping>
CategoryAnalyzer::BuildCategoryMap(const Calltree::Ptr& calltree,
                                   const std::vector<CategoryConfig::Category>& categories)
{
	int node_count = calltree->node_count;
	int untagged_index = static_cast<int>(categories.size()) - 1;

	std::vector<CategoryMapping> mappings(node_count);

	std::mutex mutex;
	ParallelTask(10000, node_count, [&](uint64_t begin, uint64_t end) {
		for (auto i = begin; i < end; ++i)
		{
			auto node = calltree->get(static_cast<int>(i));
			int cat_idx = -1;
			int matched_node = -1;

			auto cur_node = node;
			while (cur_node)
			{
				cat_idx = MatchCategory(cur_node->name, categories);
				if (cat_idx != -1)
				{
					matched_node = calltree->getIndex(cur_node);
					break;
				}
				cur_node = cur_node->parent.getNode();
			}

			if (cat_idx == -1)
				cat_idx = untagged_index;

			mappings[i].category_index = static_cast<int16_t>(cat_idx);
			mappings[i].matched_node_index = matched_node;
		}
	});

	return mappings;
}

std::vector<CategoryResult>
CategoryAnalyzer::AggregateByMap(const Calltree::Ptr& calltree,
                                 const std::vector<CategoryMapping>& category_map,
                                 const std::vector<CategoryConfig::Category>& categories)
{
	std::vector<CategoryResult> results(categories.size());
	for (size_t i = 0; i < categories.size(); ++i)
		results[i].name = categories[i].name;

	std::mutex mutex;
	int node_count = calltree->node_count;

	ParallelTask(10000, node_count, [&](uint64_t begin, uint64_t end) {
		std::vector<std::vector<CategoryAllocInfo>> local_allocs(categories.size());
		std::vector<int64_t> local_sizes(categories.size(), 0);
		std::vector<int64_t> local_counts(categories.size(), 0);

		for (auto i = begin; i < end; ++i)
		{
			auto node = calltree->get(static_cast<int>(i));
			if (node->basic_size == 0)
				continue;

			auto& mapping = category_map[i];
			int cat_idx = mapping.category_index;

			CategoryAllocInfo info;
			info.node_index = static_cast<int>(i);
			info.matched_node_index = mapping.matched_node_index;
			info.count = node->basic_count;
			info.size = node->basic_size;

			local_allocs[cat_idx].push_back(info);
			local_sizes[cat_idx] += node->basic_size;
			local_counts[cat_idx] += node->basic_count;
		}

		std::lock_guard<std::mutex> lock(mutex);
		for (size_t c = 0; c < categories.size(); ++c)
		{
			results[c].total_size += local_sizes[c];
			results[c].total_count += local_counts[c];
			results[c].allocs.insert(results[c].allocs.end(),
			                         local_allocs[c].begin(),
			                         local_allocs[c].end());
		}
	});

	for (auto& r : results)
	{
		std::stable_sort(r.allocs.begin(), r.allocs.end(),
		                 [](auto& a, auto& b) { return a.size > b.size; });
	}

	return results;
}

std::pair<std::vector<CategoryResult>, std::vector<CategoryMapping>>
CategoryAnalyzer::AnalyzeAndBuildMap(const Calltree::Ptr& calltree,
                                    const std::vector<CategoryConfig::Category>& categories)
{
	auto map = BuildCategoryMap(calltree, categories);
	auto results = AggregateByMap(calltree, map, categories);
	return { std::move(results), std::move(map) };
}

std::vector<CategoryMapping>
CategoryAnalyzer::BuildSubCategoryMap(const Calltree::Ptr& calltree,
                                      const std::vector<CategoryAllocInfo>& parent_allocs,
                                      const std::vector<CategoryConfig::SubCategory>& sub_cats)
{
	int untagged_index = -1;
	for (int i = 0; i < static_cast<int>(sub_cats.size()); ++i)
	{
		if (sub_cats[i].name == "Untagged")
		{
			untagged_index = i;
			break;
		}
	}
	if (untagged_index == -1)
		untagged_index = static_cast<int>(sub_cats.size()) - 1;

	std::vector<CategoryMapping> mappings(parent_allocs.size());

	for (size_t a = 0; a < parent_allocs.size(); ++a)
	{
		auto& alloc = parent_allocs[a];
		auto cur_node = calltree->get(alloc.node_index);
		int idx = -1;
		int matched = -1;

		while (cur_node)
		{
			for (int s = 0; s < static_cast<int>(sub_cats.size()); ++s)
			{
				for (auto& key : sub_cats[s].keys)
				{
					if (cur_node->name.find(key) != std::string::npos)
					{
						idx = s;
						matched = calltree->getIndex(cur_node);
						goto done;
					}
				}
			}
			cur_node = cur_node->parent.getNode();
		}
	done:
		if (idx == -1)
			idx = untagged_index;

		mappings[a].category_index = static_cast<int16_t>(idx);
		mappings[a].matched_node_index = (matched != -1) ? matched : alloc.matched_node_index;
	}

	return mappings;
}

std::vector<CategoryResult>
CategoryAnalyzer::AggregateSubByMap(const Calltree::Ptr& calltree,
                                    const std::vector<CategoryAllocInfo>& parent_allocs,
                                    const std::vector<CategoryMapping>& sub_map,
                                    const std::vector<CategoryConfig::SubCategory>& sub_cats)
{
	std::vector<CategoryResult> results(sub_cats.size());
	for (size_t i = 0; i < sub_cats.size(); ++i)
		results[i].name = sub_cats[i].name;

	for (size_t a = 0; a < parent_allocs.size(); ++a)
	{
		auto& alloc = parent_allocs[a];
		int sub_idx = sub_map[a].category_index;

		CategoryAllocInfo info;
		info.node_index = alloc.node_index;
		info.matched_node_index = sub_map[a].matched_node_index;
		info.count = alloc.count;
		info.size = alloc.size;

		results[sub_idx].total_size += alloc.size;
		results[sub_idx].total_count += alloc.count;
		results[sub_idx].allocs.push_back(info);
	}

	for (auto& r : results)
	{
		std::stable_sort(r.allocs.begin(), r.allocs.end(),
		                 [](auto& a, auto& b) { return a.size > b.size; });
	}

	return results;
}
