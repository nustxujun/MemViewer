#pragma once

#include <string>
#include <functional>
#include <future>
#include <atomic>
#include <mutex>
#include <map>


class TraceInstance;
class TraceInstanceBasic;
class View
{
	enum
	{
		LATEST = 0,
		NEED_INIT = 1,
		NEED_UPDATE = 2,
	};
public:
	using Ptr = std::shared_ptr<View>;
	View(std::string n);
	virtual ~View() = default;

	void Initialize();
	void Update();

	void Show(float w = 0, float h = 0, int flags = 0, std::function<bool(TraceInstanceBasic*)>&& cond = {});
	void Process();

	std::shared_ptr<TraceInstanceBasic> GetTrace(){return trace;}
	void SetTrace(std::shared_ptr<TraceInstanceBasic> t){trace =t;}

	bool isRunningTask();
	void syncTask(bool cancel = false);
	const std::string& getName(){return name;}
protected:
	virtual void InitializeImpl() = 0;
	virtual void UpdateImpl() = 0;
	virtual void ShowImpl() = 0;
	void AddTask(std::function<void()>&& func);

	std::atomic_bool require_stop = false;
private:
	std::shared_ptr<TraceInstanceBasic> trace;
	std::future<void> exec_tasks;
	std::vector<std::function<void()>> tasks;
	std::mutex mutex;
	std::string name;
	int task_flag = LATEST;

};

#define EXIT_IF_STOPED() {if (require_stop ) return;}