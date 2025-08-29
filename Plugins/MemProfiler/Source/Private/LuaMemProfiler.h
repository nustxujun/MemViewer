#pragma once 

#include "CoreMinimal.h"


//#include "LuaSubsystem.h"


class MEMPROFILER_API FLuaMemProfiler
{
public:
	static void Initialize(const TCHAR* CmdLine, const TCHAR* Dir);
	static void Start(const TCHAR* Dir);

private:
};

