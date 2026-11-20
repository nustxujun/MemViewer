#pragma once

#include "CoreTypes.h"

#include <vector>
#include <map>
#include <future>

class TraceInstanceBasic
{
public: 
	virtual ~TraceInstanceBasic() = default;
	using Ptr = std::shared_ptr<TraceInstanceBasic>;

	virtual Calltree::Ptr getCalltree()= 0;
	virtual Calltree::Ptr getLuaCalltree() = 0;
	virtual const std::vector<RhiInfo>& getRHIs() = 0;
	virtual const std::vector<ObjectInfo>& getObjects() = 0;
	virtual TraceData::Ptr getTraceData() = 0;


	TraceInstanceBasic::Ptr getDiff(TraceInstanceBasic::Ptr file);

	enum State
	{
		Unprepared,
		Updating,

		Updated,
		Prepared
	};

	bool isPrepared() { return state == Prepared; }
	bool isUpdated() { return state == Updated; }
	void Prepare() { state = Prepared; }

	bool isOnState(int s) { return state == s; }
	bool isNotOnstate(int s) { return state != s; }

	int state = Unprepared;

};

class TraceInstanceDiff : public TraceInstanceBasic
{
public:
	using Ptr = std::shared_ptr<TraceInstanceDiff>;

	TraceInstanceDiff(TraceData::Ptr data) :trace_data(data)
	{
	}

	virtual FrameRange getTraceRange() { return trace_data->max_range; }

	Calltree::Ptr getCalltree() { return trace_data->calltree; }
	Calltree::Ptr getLuaCalltree() { return trace_data->lua_calltree; }
	virtual const std::vector<AllocInfo>& getAllocs() { return trace_data->allocs; }
	virtual const std::vector<RhiInfo>& getRHIs() { return trace_data->rhis; }
	virtual const std::vector<ObjectInfo>& getObjects() { return trace_data->objects; }
	virtual TraceData::Ptr getTraceData() {return trace_data;};

	TraceData::Ptr trace_data;

	static Ptr CreateFromFile(const std::string& path, std::function<void(Ptr)>&& callback = {});
};

class TraceInstance: public TraceInstanceBasic
{
	using Ptr = std::shared_ptr<TraceInstance>;
	static std::map<std::string, TraceData::Ptr> Traces;
public:
	TraceInstance(TraceData::Ptr d):data(d)
	{
		ranged_data = std::make_shared<TraceData>();
		selected_range = data->max_range;
		(*ranged_data) = *data;
	}

	static size_t getNumTraces()
	{ 
		return Traces.size();
	}

	static TraceInstance::Ptr Create(const std::string& path, std::function<void(Ptr)>&& callback = {});
	static void ParseRange(int begin, int end, TraceInstance::Ptr trace);

	FrameRange getTraceRange(){return data->max_range;}
	FrameRange getSelectRange(){return selected_range;}




	Calltree::Ptr getCalltree(){return ranged_data->calltree;}
	Calltree::Ptr getLuaCalltree() { return ranged_data->lua_calltree; }
	const std::vector<AllocInfo>& getTotalAllocs() { return data->allocs; }
	const std::vector<AllocInfo>& getAllocs(){return ranged_data->allocs;}

	const std::vector<AllocInfo>& getTotalLuaAllocs() { return data->lua_allocs; }
	const std::vector<AllocInfo>& getLuaAllocs() { return ranged_data->lua_allocs; }

	const std::vector<TotalInfo>& getMemoryInfos(){return data->memoryinfos;}
	const std::vector< Checkpoint>& getCheckpoints(){return data->checkpoints;}

	const std::vector<RhiInfo>& getRHIs(){return ranged_data->rhis;}

	const std::vector<ObjectInfo>& getObjects(){return ranged_data->objects;}

	virtual TraceData::Ptr getTraceData() { return ranged_data; };



private:
	TraceData::Ptr data;


	TraceData::Ptr ranged_data;

	FrameRange selected_range;

};
