

#include <vector>
#include <string>
#include <map>
#include <functional>
#include <filesystem>
#include <fstream>
#include "TraceParser.h"
#include "TraceInstance.h"
#include "Concurrency.h"


void generate_spans(const std::string& path);


static std::vector<std::function<void()>> post_process;
static std::filesystem::path output_dir;
static std::map<std::string, std::function<void(const std::string&)>> cmds = 
{
	{"-s",[](auto path){ post_process.push_back([=](){generate_spans(path);}); }},
	{"-o",[](auto path){ output_dir= path; }
		
	},
};


void runCommand(const std::vector<std::string>& params)
{
	auto i = params.begin();

	while(i != params.end())
	{
		if ((*i)[0] == '-')
		{
			auto cmd = cmds.find(*i);
			if (cmd != cmds.end())
			{
				i++;
				if (i != params.end())
					cmd->second(*i);
				else
				{
					cmd->second("true");
					break;
				}
			}
			
		}

		i++;
	}



	for (auto& p : post_process)
	{
		p();
	}
}

void generate_spans(const std::string& path) 
{
	TraceData::Ptr trace_data = std::make_shared<TraceData>();

	TraceData::ParseTraceFile(path,trace_data.get() );


	if(trace_data->checkpoints.size() == 0)
		return;

	auto file_name = std::filesystem::path(path).filename().stem();


	auto count = trace_data->checkpoints.size();

	std::cout << "begin generate spans" << std::endl;
	//for (auto& cp : trace_data->checkpoints)
	// ParallelTask([&](int idx)
	for (int idx = 0; idx < trace_data->checkpoints.size(); ++idx)
	{
			auto& cp = trace_data->checkpoints[idx];
		TraceData::Ptr ranged_data = std::make_shared<TraceData>();
		TraceData::ParseRange(1, cp.frameid, trace_data.get(), ranged_data.get());


		auto output_path = output_dir / std::format("{}_span_{}", file_name.string(),UTF8ToGBK( cp.text));
		auto temp_file = std::format("temp_file{}", idx);
		std::fstream output(temp_file, std::ios::out | std::ios::binary);

		TraceData::SerializeTraceSpan(ranged_data.get(),
			[&](uint8_t* data, int size) {
				output.write((const char*)data, size);
			}, true);

		output.close();

		run_cmd(std::format("tar -czvf {}.zip temp_file{}", output_path.string(), idx));
		std::filesystem::remove(temp_file);
	}
	//, count);

}