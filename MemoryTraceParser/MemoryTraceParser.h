#pragma once



#ifdef _MSC_VER
#define PLATFORM_WINDOWS 1
#elif __APPLE__
#define PLATFORM_MAC 1
#else
#endif

#define IOS_PARSER 1
#define WIN64_PARSER 0


#if IOS_PARSER && WIN64_PARSER
#error "exist parser more than 1"
#endif

#include <iostream>
#include <chrono>
#include <format>

struct ModuleInfo
{
	uint64_t base_addr;
	uint32_t size;
	std::string name;
};

#define Log(...) std::cout << std::format(__VA_ARGS__) << std::endl
#define Error( ...) {std::cout << "\n\nerror: " <<std::format(__VA_ARGS__) << std::endl;exit(-1);}
#define Assert(cond, ...) if (!(cond)) {Error(__VA_ARGS__);}


struct Timer
{
    using timer_t = std::chrono::high_resolution_clock;
    timer_t::time_point tp;
    std::string name;

    ~Timer()
    {
        if (!name.empty())
            end();
    }
    
    void begin(std::string n)
    {
        name = std::move(n);
        Log("{0} ...\n", name);
        tp = timer_t::now();
    }
 
    void end()
    {
        auto diff = (timer_t::now() - tp).count() / 1000000000.0f;
        Log("done. ({0:.4f} s)\n", diff);
        name = {};
    }
    
    void then(std::string n)
    {
        end();
        begin(std::move(n));
    }
};
