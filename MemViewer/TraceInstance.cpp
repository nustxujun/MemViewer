#include "TraceInstance.h"
#include "Concurrency.h"

std::map<std::string, TraceData::Ptr> TraceInstance::Traces;

TraceInstance::Ptr TraceInstance::Create(const std::string& path, std::function<void(Ptr)>&& callback)
{
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

	for (size_t i = 0, j = 0; i < objs.size() || j < objs2.size();)
	{
		if (i < objs.size() && j < objs2.size())
		{
			auto& o1 = objs[i];
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
		else if (i < objs.size())
		{
			ret_data->objects.push_back(objs[i]);
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
