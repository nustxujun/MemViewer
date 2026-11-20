#include "CoreTypes.h"
#include "Concurrency.h"
#include <algorithm>

Node* NodeRef::getNode() const
{
	if (tree)
		return (*tree)[index];
	else
		return nullptr;
}

NodeRef::NodeRef(std::shared_ptr<Calltree> parent, Node* node)
{
	tree = parent;
	index = parent->getIndex(node);
}

CustomData::Ptr CustomData::createCustomData(int type)
{
	return std::make_shared<CustomData>();
}

Calltree::Ptr Calltree::clone() const
{
	auto tree = Calltree::create(node_count);

	tree->root[0] = root[0];
	tree->root[0].parent = {};
	for (auto& c : tree->root[0].children)
		c << tree;

	for (int i = 1; i < node_count; ++i)
	{
		new (tree->root + i) Node();
		tree->root[i] = root[i];
		tree->root[i].parent << tree;

		for (auto& c : tree->root[i].children)
		{
			c << tree;
		}
	}

	return tree;
}

Calltree::Ptr Calltree::diff(Ptr tree1, Ptr tree2)
{
	auto tree = create(tree1->node_count + tree2->node_count);
	int cur = 1;

	auto create_node = [&]() {
		new (tree->root + cur) Node();
		return NodeRef{tree, cur++};
	};

	auto merge_node = [&](Node* n1, Node* n2, NodeRef node, bool reversed, auto& merge_func) -> void {
		std::stable_sort(n1->children.begin(), n1->children.end(), [](auto& a, auto& b) {
			return a->name < b->name;
		});

		if (n2)
		{
			std::stable_sort(n2->children.begin(), n2->children.end(), [](auto& a, auto& b) {
				return a->name < b->name;
			});
		}

		std::vector<NodeRef>::iterator beg1, beg2, end1, end2;
		if (n2)
		{
			if (n1->children.size() < n2->children.size())
			{
				beg1 = n2->children.begin();
				end1 = n2->children.end();
				beg2 = n1->children.begin();
				end2 = n1->children.end();
				reversed = !reversed;
			}
			else
			{
				beg1 = n1->children.begin();
				end1 = n1->children.end();
				beg2 = n2->children.begin();
				end2 = n2->children.end();
			}
		}
		else
		{
			beg1 = n1->children.begin();
			end1 = n1->children.end();
			beg2 = n1->children.end();
			end2 = n1->children.end();
		}

		while (beg1 != end1 || beg2 != end2)
		{
			int cmp = 0;
			if (beg1 != end1 && beg2 != end2)
			{
				auto& name1 = (*beg1)->name;
				auto& name2 = (*beg2)->name;
				auto ret = name1 <=> name2;
				if (ret == std::strong_ordering::less)
					cmp = -1;
				else if (ret == std::strong_ordering::equal)
					cmp = 0;
				else
					cmp = 1;
			}

			if ((beg1 != end1 && beg2 == end2) || cmp < 0)
			{
				auto n = create_node();
				n->parent = node;
				n->name = (*beg1)->name;
				if (!reversed)
				{
					n->basic_size = -(*beg1)->basic_size;
					n->basic_count = -(*beg1)->basic_count;
					node->children.push_back(n);
					merge_func(**beg1, nullptr, n, false, merge_func);
				}
				else
				{
					n->basic_size = (*beg1)->basic_size;
					n->basic_count = (*beg1)->basic_count;
					node->children.push_back(n);
					merge_func(**beg1, nullptr, n, true, merge_func);
				}
				++beg1;
			}
			else if ((beg1 != end1 && beg2 != end2) && cmp == 0)
			{
				auto n = create_node();
				n->parent = node;
				n->name = (*beg1)->name;
				if (!reversed)
				{
					n->basic_size = -(*beg1)->basic_size + (*beg2)->basic_size;
					n->basic_count = -(*beg1)->basic_count + (*beg2)->basic_count;
					node->children.push_back(n);
					merge_func(**beg1, **beg2, n, false, merge_func);
				}
				else
				{
					n->basic_size = (*beg1)->basic_size - (*beg2)->basic_size;
					n->basic_count = (*beg1)->basic_count - (*beg2)->basic_count;
					node->children.push_back(n);
					merge_func(**beg1, **beg2, n, true, merge_func);
				}
				++beg1;
				++beg2;
			}
			else if ((beg2 != end2 && beg1 == end1) || cmp > 0)
			{
				auto n = create_node();
				n->parent = node;
				n->name = (*beg2)->name;
				if (reversed)
				{
					n->basic_size = -(*beg2)->basic_size;
					n->basic_count = -(*beg2)->basic_count;
					node->children.push_back(n);
					merge_func(**beg2, nullptr, n, false, merge_func);
				}
				else
				{
					n->basic_size = (*beg2)->basic_size;
					n->basic_count = (*beg2)->basic_count;
					node->children.push_back(n);
					merge_func(**beg2, nullptr, n, true, merge_func);
				}
				++beg2;
			}
			else
			{
				Error("diff merge error");
			}
		}

		node->size = node->basic_size;
		node->count = node->basic_count;
	};

	merge_node(tree1->root, tree2->root, NodeRef{tree, 0}, false, merge_node);
	tree->node_count = cur;
	return tree;
}

void TraceData::ShrinkCalltree()
{
	auto tree = calltree;
	auto recal = [](auto node, auto& recal) -> void {
		node->count = node->basic_count;
		node->size = node->basic_size;
		node->is_matched = true;
		for (auto& c : node->children)
		{
			recal(c, recal);
			node->count += c->count;
			node->size += c->size;
		}
	};

	recal(NodeRef{tree, 0}, recal);

	std::map<int, std::vector<NodeRef*>> node_map;

	for (auto& o : objects)
	{
		node_map[o.node.index].push_back(&o.node);
		o.node->is_matched = false;
	}

	for (auto& a : allocs)
	{
		node_map[a.node.index].push_back(&a.node);
		a.node->is_matched = false;
	}

	std::vector<int> freelist;
	auto add_free = [&](auto node, auto& add) -> bool {
		bool free = true;
		auto iter = node->children.begin();
		for (; iter != node->children.end();)
		{
			if (add(*iter, add))
			{
				free = false;
				iter++;
			}
			else
			{
				iter = node->children.erase(iter);
			}
		}

		if (free)
		{
			if (node->is_matched)
			{
				freelist.push_back(node.index);

				for (auto& c : node->children)
				{
					c->parent.index = 0;
					c->parent.tree.reset();
				}

				return false;
			}
		}
		node->is_matched = false;
		return true;
	};

	add_free(NodeRef(tree, 0), add_free);
	std::stable_sort(freelist.begin(), freelist.end());

	if (freelist.empty())
		return;

	auto num_node = tree->node_count;
	auto ret_tree = tree->clone();
	int used = num_node - 1;

	for (auto free_idx : freelist)
	{
		for (; used > free_idx && ret_tree->root[used].is_matched; --used)
		{
		}

		if (used <= free_idx)
			break;

		auto& used_node = ret_tree->root[used];
		auto& free_node = ret_tree->root[free_idx];

		for (auto& c : used_node.parent->children)
		{
			if (c.index == used)
			{
				c.index = free_idx;
				break;
			}
		}
		for (auto& c : used_node.children)
		{
			c->parent.index = free_idx;
		}

		for (auto& node : node_map[used])
		{
			node->index = free_idx;
		}

		std::swap(used_node, free_node);
		used--;
	}

	ret_tree->node_count = std::max<int>(1, num_node - (int)freelist.size());

	for (auto i = 0; i < ret_tree->node_count; ++i)
	{
		ret_tree->root[i].parent << ret_tree;
		for (auto& c : ret_tree->root[i].children)
		{
			c << ret_tree;
		}
	}

	calltree = ret_tree->clone();
}

TraceData::Ptr TraceData::GetDiff(const TraceData& data1, const TraceData& data2)
{
	auto ret_data = std::make_shared<TraceData>();

	ret_data->calltree = Calltree::diff(
		std::const_pointer_cast<Calltree>(data1.calltree),
		std::const_pointer_cast<Calltree>(data2.calltree));

	if (data1.lua_calltree && data2.lua_calltree)
	{
		ret_data->lua_calltree = Calltree::diff(
			std::const_pointer_cast<Calltree>(data1.lua_calltree),
			std::const_pointer_cast<Calltree>(data2.lua_calltree));
	}

	auto objs1 = data1.objects;
	auto objs2 = data2.objects;

	std::stable_sort(objs1.begin(), objs1.end(), [](auto& a, auto& b) {
		return a.name < b.name;
	});
	std::stable_sort(objs2.begin(), objs2.end(), [](auto& a, auto& b) {
		return a.name < b.name;
	});

	for (size_t i = 0, j = 0; i < objs1.size() || j < objs2.size();)
	{
		if (i < objs1.size() && j < objs2.size())
		{
			auto& o1 = objs1[i];
			auto& o2 = objs2[j];

			if (o1.name < o2.name)
			{
				ret_data->objects.push_back(o1);
				ret_data->objects.back().node << ret_data->calltree;
				ret_data->objects.back().state = -1;
				i++;
			}
			else if (o2.name < o1.name)
			{
				ret_data->objects.push_back(o2);
				ret_data->objects.back().node << ret_data->calltree;
				ret_data->objects.back().state = 0;
				j++;
			}
			else
			{
				i++;
				j++;
			}
		}
		else if (i < objs1.size())
		{
			ret_data->objects.push_back(objs1[i]);
			ret_data->objects.back().node << ret_data->calltree;
			ret_data->objects.back().state = -1;
			i++;
		}
		else
		{
			ret_data->objects.push_back(objs2[j]);
			ret_data->objects.back().node << ret_data->calltree;
			ret_data->objects.back().state = 0;
			j++;
		}
	}

	std::stable_sort(ret_data->objects.begin(), ret_data->objects.end(), [](auto& a, auto& b) {
		return a.begin < b.begin;
	});

	ret_data->rhis = data1.rhis;
	for (auto& rhi : ret_data->rhis)
	{
		rhi.size = -rhi.size;
	}
	ret_data->rhis.reserve(ret_data->rhis.size() + data2.rhis.size());
	ret_data->rhis.insert(ret_data->rhis.end(), data2.rhis.begin(), data2.rhis.end());
	std::stable_sort(ret_data->rhis.begin(), ret_data->rhis.end(), [](auto& a, auto& b) {
		return a.begin < b.begin;
	});

	ret_data->max_range = {
		std::min(data1.max_range.end, data2.max_range.end),
		std::max(data1.max_range.end, data2.max_range.end)
	};

	return ret_data;
}

NodeRef CloneTree(NodeRef root)
{
	if (!root.tree)
		return {};

	auto cloned = root.tree->clone();
	return NodeRef{cloned, root.index};
}

void TraceSession::SetRange(int begin, int end)
{
	int begin_frame = std::min(begin, end);
	int end_frame = std::max(begin, end);

	selected_range.begin = begin_frame;
	selected_range.end = end_frame;

	TraceData::ParseRange(begin_frame, end_frame, &full_data, &ranged_data);
}
