#pragma once 

#include <map>

class FIOSMemoryInfo
{
public:
	static std::map<std::string, int64> GetCustomMemoryInfo();
};



using FGenericPlatformMemoryInfo = FIOSMemoryInfo;
