#include "FrameParser.h"
#include "TraceParser.h"
#include "TraceInstance.h"
#include "MemViewer.h"
#include "Utils.h"
#include "Concurrency.h"
#include "View.h"
#include <vector>
#include <algorithm>
#include <format>
#include <string>
#include <ranges>


static void parseAllocsRange(int begin, int end, const TraceData* data, TraceData* ranged_data)
{
	auto begin_frame = begin;
	auto end_frame = end;

	auto end_pos = std::upper_bound(data->allocs.begin(), data->allocs.end(), (uint32_t)end_frame, [](auto frame, auto& a) {
		return frame < a.start;
		});

	auto tree = data->calltree->clone();
	tree->resetNodes();
	ranged_data->allocs.clear();
	for (auto i = data->allocs.begin(); i != end_pos; ++i)
	{
		if (i->end < begin_frame || (i->start >= begin_frame && i->end <= end_frame))
			continue;

		auto alloc = *i;
		auto& node = alloc.node;
		node << tree;

		if (i->end <= end_frame)
		{
			node->basic_count--;
			node->basic_size -= i->size;
			alloc.size = -alloc.size;

		}
		else if (i->start >= begin_frame)
		{
			node->basic_count++;
			node->basic_size += i->size;
		}
		ranged_data->allocs.push_back(std::move(alloc));

	}


	auto cal_size = [](Node* node, auto& cal)->void
		{
			for (auto& child : node->children)
			{
				cal(*child, cal);
				node->size += child->size;
				node->count += child->count;
			}


			std::stable_sort(node->children.begin(), node->children.end(), [&](auto n1, auto n2)
				{
					return n1->size > n2->size;
				});

			node->size += node->basic_size;
			node->count += node->basic_count;

		};

	cal_size(tree->get(0), cal_size);

	ranged_data->calltree = std::move(tree);

}

static void parseOthersRange(int begin, int end, const TraceData* data, TraceData* ranged_data)
{
	{
		auto end_pos = std::upper_bound(data->objects.begin(), data->objects.end(), (uint32_t)end, [](auto frame, auto& a) {
			return frame < a.begin;
			});


		auto& objs = ranged_data->objects;
		objs.clear();

		for (auto i = data->objects.begin(); i != end_pos; ++i)
		{
			if (i->end < begin || (i->begin >= begin && i->end <= end))
				continue;

			auto cpy = *i;
			if (i->end <= end)
			{
				cpy.state = -1;
				objs.push_back(std::move(cpy));
			}
			else if (i->begin >= begin)
			{
				cpy.state = 0;
				objs.push_back(std::move(cpy));
			}
		}
	}
	{
		auto end_pos = std::upper_bound(data->rhis.begin(), data->rhis.end(), (uint32_t)end, [](auto frame, auto& a) {
			return frame < a.begin;
			});


		auto& rhis = ranged_data->rhis;
		rhis.clear();

		for (auto i = data->rhis.begin(); i != end_pos; ++i)
		{
			if (i->end < begin || (i->begin >= begin && i->end <= end))
				continue;

			auto cpy = *i;
			if (i->end <= end)
			{
				cpy.size = -cpy.size;
				rhis.push_back(std::move(cpy));
			}
			else if (i->begin >= begin)
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
		case 0: parseAllocsRange(begin_frame, end_frame,trace, ranged_trace); return;
		case 1: parseOthersRange(begin_frame, end_frame, trace, ranged_trace); return;
		}
	}, 2);
	for (auto& o : ranged_trace->objects)
	{
		o.node << ranged_trace->calltree;
	}
	ranged_trace->ShrinkCalltree();
	ranged_trace->max_range = { begin_frame, end_frame };
}


void TraceInstance::ParseRange(int begin, int end, TraceInstance::Ptr trace)
{
	trace->state = Updating;

	auto begin_frame = std::min(begin, end);
	auto end_frame = std::max(begin, end);

	trace->selected_range.begin = begin_frame;
	trace->selected_range.end = end_frame;

	AsyncTask("update range", [=]() {


		TraceData::ParseRange(begin_frame, end_frame, trace->data.get(),trace->ranged_data.get());
		

		trace->state = Updated;
	});
}


//void TraceInstance::parseAllocsRange(int begin, int end)
//{
//	auto begin_frame = begin;
//	auto end_frame = end;
//
//	auto end_pos = std::upper_bound(data->allocs.begin(), data->allocs.end(), (uint32_t)end_frame, [](auto frame, auto& a) {
//		return frame < a.start;
//	});
//
//	auto tree = data->calltree->clone();
//	tree->resetNodes();
//	ranged_data->allocs.clear();
//	for (auto i = data->allocs.begin(); i != end_pos; ++i)
//	{
//		if ( i->end < begin_frame || ( i->start >= begin_frame && i->end <= end_frame) )
//			continue;
//
//		auto node = i->node;
//		node << tree;
//		
//		auto alloc = *i;
//		if (i->end <= end_frame)
//		{
//			node->basic_count--;
//			node->basic_size-=i->size;
//			alloc.size = -alloc.size;
//
//		}
//		else if (i->start >= begin_frame)
//		{
//			node->basic_count++;
//			node->basic_size += i->size;
//		}
//		ranged_data->allocs.push_back(std::move(alloc));
//
//	}
//
//
//	auto cal_size = [](Node* node, auto& cal)->void
//	{
//		for (auto& child : node->children)
//		{
//			cal(*child,cal);
//			node->size += child->size;
//			node->count += child->count;
//		}
//
//
//		std::stable_sort(node->children.begin(), node->children.end(), [&](auto n1, auto n2)
//			{
//				return n1->size > n2->size;
//			});
//
//		node->size += node->basic_size;
//		node->count += node->basic_count;
//
//	};
//
//	cal_size(tree->get(0), cal_size);
//
//	tree = Calltree::shrink(tree); 
//	ranged_data->calltree = std::move(tree);
//
//}
//
//
//void TraceInstance::parseOthersRange(int begin, int end)
//{
//	{
//		auto end_pos = std::upper_bound(data->objects.begin(), data->objects.end(), (uint32_t)end, [](auto frame, auto& a) {
//			return frame < a.begin;
//		});
//
//
//		auto& objs = ranged_data->objects;
//		objs.clear();
//
//		for (auto i = data->objects.begin(); i != end_pos; ++i)
//		{
//			if (i->end < begin || (i->begin >= begin && i->end <= end))
//				continue;
//
//			auto cpy = *i;
//			if (i->end <= end)
//			{
//				cpy.state = -1;
//				objs.push_back(std::move(cpy));
//			}
//			else if (i->begin >= begin)
//			{
//				cpy.state = 0;
//				objs.push_back(std::move(cpy));
//			}
//		}
//	}
//	{
//		auto end_pos = std::upper_bound(data->rhis.begin(), data->rhis.end(), (uint32_t)end, [](auto frame, auto& a) {
//			return frame < a.begin;
//			});
//
//
//		auto& rhis = ranged_data->rhis;
//		rhis.clear();
//
//		for (auto i = data->rhis.begin(); i != end_pos; ++i)
//		{
//			if (i->end < begin || (i->begin >= begin && i->end <= end))
//				continue;
//
//			auto cpy = *i;
//			if (i->end <= end)
//			{
//				cpy.size = -cpy.size;
//				rhis.push_back(std::move(cpy));
//			}
//			else if (i->begin >= begin)
//			{
//				rhis.push_back(std::move(cpy));
//			}
//		}
//	}
//
//}
//
