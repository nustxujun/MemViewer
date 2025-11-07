#pragma once 

#include <map>

class FAndroidMemoryInfo
{
public:
	static std::map<std::string, int64> GetCustomMemoryInfo()
    {
        return {};
    }
};



using FGenericPlatformMemoryInfo = class FAndroidMemoryInfo;
