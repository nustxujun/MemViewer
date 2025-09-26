#pragma once 

#include <map>

class FWindowsMemoryInfo
{
public:
	static std::map<std::string, int64> GetCustomMemoryInfo();
};



using FGenericPlatformMemoryInfo = FWindowsMemoryInfo;
