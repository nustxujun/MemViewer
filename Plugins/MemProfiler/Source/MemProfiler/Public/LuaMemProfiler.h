#pragma once 

#include "CoreMinimal.h"


//#include "LuaSubsystem.h"


class FLuaMemProfiler
{
public:
	static void Initialize(const TCHAR* CmdLine, const TCHAR* Dir);
	static void Start(const TCHAR* Dir);

	MEMPROFILER_API static bool IsEnabled();
	MEMPROFILER_API static void* Allocator(void* ud, void* ptr, size_t osize, size_t nsize);
	MEMPROFILER_API static void CallEvent(const struct lua_State* Thread, int Event, const TCHAR* Content);
private:
};

