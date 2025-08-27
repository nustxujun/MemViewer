#include "TraceInstance.h"
#include "Concurrency.h"

std::map<std::string, TraceData::Ptr> TraceInstance::Traces;
void TraceData::ShrinkCalltree()
{
	auto tree = calltree;
	auto recal = [](auto node, auto& recal)->void
		{
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

	recal(NodeRef{ tree,0 }, recal);


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
	auto add_free = [&](auto node, auto& add)->bool
		{
			bool free = true;
			auto iter = node->children.begin();
			for (; iter != node->children.end(); )
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

				if ( node->is_matched)
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
		return ;

	auto num_node = tree->node_count;
	auto ret_tree = tree->clone();
	int used = num_node - 1;

	for (auto free : freelist)
	{
		for (; used > free && ret_tree->root[used].is_matched; --used)
		{
		}

		if (used <= free)
			break;


		auto& used_node = ret_tree->root[used];
		auto& free_node = ret_tree->root[free];


		for (auto& c : used_node.parent->children)
		{
			if (c.index == used)
			{
				c.index = free;
				break;
			}
		}
		for (auto& c : used_node.children)
		{
			c->parent.index = free;
		}

		for (auto& node : node_map[used])
		{
			node->index = free;
		}


		std::swap(used_node, free_node);
		used--;

	}



	ret_tree->node_count = num_node - freelist.size();

	for (int i = ret_tree->node_count; i < tree->node_count; ++i)
	{
		Assert(ret_tree->root[i].is_matched, "invalid");
	}

	for (auto i = 0; i < ret_tree->node_count; ++i)
	{
		ret_tree->root[i].parent << ret_tree;


		for (auto& c : ret_tree->root[i].children)
		{
			c << ret_tree;
		}
	}
	//for (auto& list : node_map)
	//{
	//	for (auto& node : list.second)
	//	{
	//		node->tree = ret_tree;
	//	}
	//}

	calltree = ret_tree->clone();
}


TraceInstance::Ptr TraceInstance::Create(const std::string& path, std::function<void(Ptr)>&& callback)
{
	if (Traces.contains(path))
	{
		auto trace = std::make_shared<TraceInstance>(Traces[path]);
		AsyncTask("ParseData",[trace](){return trace;},[ cb = std::move(callback)](auto trace) {
			trace->state = Prepared;
			cb(trace);
		});
		return trace;
	}


	auto data = std::make_shared<TraceData>();
	auto trace = std::make_shared<TraceInstance>(data);

	Traces[path] = data;

	AsyncTask("ParseData", [path, data, trace]()->Ptr 
	{
		if (TraceData::ParseTraceFile(path, data.get()))
			return trace;
		else
			return {};
	}, 
	[callback = std::move(callback)](Ptr ret){
		if (ret)
		{
			ret->state = Prepared;
			if (callback) callback(ret);
		}
	});

	return trace;
}

TraceInstanceBasic::Ptr TraceInstanceBasic::getDiff(TraceInstanceBasic::Ptr file)
{
	auto ret_data = std::make_shared<TraceData>();

	ret_data->calltree = Calltree::diff(getCalltree(), file->getCalltree());
	if (getLuaCalltree() && file->getLuaCalltree())
		ret_data->lua_calltree = Calltree::diff(getLuaCalltree(), file->getLuaCalltree());

	auto objs = getObjects();
	auto objs2 = file->getObjects();

	std::stable_sort(objs.begin(), objs.end(), [](auto& a, auto& b){
		return a.name < b.name;
	});

	std::stable_sort(objs2.begin(), objs2.end(), [](auto& a, auto& b) {
		return a.name < b.name;
	});

	for (int i = 0, j = 0; i < objs.size() && j < objs2.size();)
	{
		auto& o1 = objs[i];
		auto& o2 = objs2[j];

		if (o1.name < o2.name)
		{
			ret_data->objects.push_back(o1);

			auto& new_obj = ret_data->objects[ret_data->objects.size() - 1];
			new_obj.node << ret_data->calltree;
			new_obj.state = -1;
			i++;
		}
		else if (o2.name < o1.name)
		{
			ret_data->objects.push_back(o2);
			auto& new_obj = ret_data->objects[ret_data->objects.size() - 1];
			new_obj.node << ret_data->calltree;
			new_obj.state = 0;
			j++;
		}
		else
		{
			i++;
			j++;
		}
	}

	std::stable_sort(ret_data->objects.begin(), ret_data->objects.end(), [&](auto& a, auto& b) {
		return a.begin < b.begin;
	});


	ret_data->rhis = getRHIs();
	for (auto& rhi : ret_data->rhis)
	{
		rhi.size = -rhi.size;
	}
	ret_data->rhis.reserve(ret_data->rhis.size() + file->getRHIs().size());
	ret_data->rhis.insert(ret_data->rhis.end(), file->getRHIs().begin(), file->getRHIs().end());
	std::stable_sort(ret_data->rhis.begin(), ret_data->rhis.end(),[](auto& a, auto& b){
		return a.begin < b.begin;
	});

	ret_data->max_range = {
		std::min(getTraceData()->max_range.end, file->getTraceData()->max_range.end),
		std::max(getTraceData()->max_range.end, file->getTraceData()->max_range.end)
	};


	//for (auto obj : file->getObjects())
	//{
	//	auto it = std::lower_bound(ret_data->objects.begin(), ret_data->objects.end(), obj.begin, [](auto& a, auto val) {
	//		return a.begin < val;
	//	});
	//	ret_data->objects.insert(it, std::move(obj));
	//}

	//for (auto rhi : file->getRHIs())
	//{
	//	auto it = std::lower_bound(ret_data->rhis.begin(), ret_data->rhis.end(), rhi.begin, [](auto& a, auto val) {
	//		return a.begin < val;
	//		});
	//	ret_data->rhis.insert(it, std::move(rhi));
	//}

	return std::make_shared<TraceInstanceDiff>(ret_data);
}


TraceInstanceDiff::Ptr TraceInstanceDiff::CreateFromFile(const std::string& path, std::function<void(Ptr)>&& callback )
{

	auto data = std::make_shared<TraceData>();
	auto trace = std::make_shared<TraceInstanceDiff>(data);


	AsyncTask("ParseData", [path, data, trace]()->Ptr {
		if (TraceData::ParseTraceSpanFile(path, data.get()))
		{ 
			return trace;
		}
		return {};
		}, [callback = std::move(callback)](Ptr ret) {
			if (ret)
			{
				ret->state = Prepared;
				if (callback) callback(ret);
			}
		});


	return trace;
}

