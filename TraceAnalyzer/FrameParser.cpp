#include "CoreTypes.h"
#include "Concurrency.h"
#include <algorithm>

static void parseAllocsRange(int begin, int end,
	const std::vector<AllocInfo>& allocs,
	Calltree::Ptr calltree,
	std::vector<AllocInfo>& ranged_allocs,
	Calltree::Ptr& ranged_tree)
{
	if (!calltree)
		return;

	auto begin_frame = begin;
	auto end_frame = end;

	auto end_pos = std::upper_bound(allocs.begin(), allocs.end(), (uint32_t)end_frame,
		[](auto frame, auto& a) { return frame < a.start; });

	auto tree = calltree->clone();
	tree->resetNodes();
	ranged_allocs.clear();

	for (auto i = allocs.begin(); i != end_pos; ++i)
	{
		if (i->end < (uint32_t)begin_frame || (i->start >= (uint32_t)begin_frame && i->end <= (uint32_t)end_frame))
			continue;

		auto alloc = *i;
		auto& node = alloc.node;
		node << tree;

		if (i->end <= (uint32_t)end_frame)
		{
			node->basic_count--;
			node->basic_size -= i->size;
			alloc.size = -(int32_t)alloc.size;
		}
		else if (i->start >= (uint32_t)begin_frame)
		{
			node->basic_count++;
			node->basic_size += i->size;
		}
		ranged_allocs.push_back(std::move(alloc));
	}

	auto cal_size = [](Node* node, auto& cal) -> void {
		for (auto& child : node->children)
		{
			cal(*child, cal);
			node->size += child->size;
			node->count += child->count;
		}

		std::stable_sort(node->children.begin(), node->children.end(),
			[&](auto n1, auto n2) { return n1->size > n2->size; });

		node->size += node->basic_size;
		node->count += node->basic_count;
	};

	cal_size(tree->get(0), cal_size);
	ranged_tree = std::move(tree);
}

static void parseOthersRange(int begin, int end, const TraceData* data, TraceData* ranged_data)
{
	{
		auto end_pos = std::upper_bound(data->objects.begin(), data->objects.end(), (uint32_t)end,
			[](auto frame, auto& a) { return frame < a.begin; });

		auto& objs = ranged_data->objects;
		objs.clear();

		for (auto i = data->objects.begin(); i != end_pos; ++i)
		{
			if (i->end < (uint32_t)begin || (i->begin >= (uint32_t)begin && i->end <= (uint32_t)end))
				continue;

			auto cpy = *i;
			if (i->end <= (uint32_t)end)
			{
				cpy.state = -1;
				objs.push_back(std::move(cpy));
			}
			else if (i->begin >= (uint32_t)begin)
			{
				cpy.state = 0;
				objs.push_back(std::move(cpy));
			}
		}
	}
	{
		auto end_pos = std::upper_bound(data->rhis.begin(), data->rhis.end(), (uint32_t)end,
			[](auto frame, auto& a) { return frame < a.begin; });

		auto& rhis = ranged_data->rhis;
		rhis.clear();

		for (auto i = data->rhis.begin(); i != end_pos; ++i)
		{
			if (i->end < (uint32_t)begin || (i->begin >= (uint32_t)begin && i->end <= (uint32_t)end))
				continue;

			auto cpy = *i;
			if (i->end <= (uint32_t)end)
			{
				cpy.size = -cpy.size;
				rhis.push_back(std::move(cpy));
			}
			else if (i->begin >= (uint32_t)begin)
			{
				rhis.push_back(std::move(cpy));
			}
		}
	}
}

void TraceData::ParseRange(int begin, int end, const TraceData* trace, TraceData* ranged_trace)
{
	auto begin_frame = std::min(begin, end);
	auto end_frame = std::max(begin, end);

	ParallelTask([=](int i) {
		switch (i)
		{
		case 0:
			parseAllocsRange(begin_frame, end_frame, trace->allocs, trace->calltree,
				ranged_trace->allocs, ranged_trace->calltree);
			return;
		case 1:
			parseAllocsRange(begin_frame, end_frame, trace->lua_allocs, trace->lua_calltree,
				ranged_trace->lua_allocs, ranged_trace->lua_calltree);
			return;
		case 2:
			parseOthersRange(begin_frame, end_frame, trace, ranged_trace);
			return;
		}
	}, 3);

	for (auto& o : ranged_trace->objects)
	{
		o.node << ranged_trace->calltree;
	}
	ranged_trace->ShrinkCalltree();
	ranged_trace->max_range = {begin_frame, end_frame};
}
