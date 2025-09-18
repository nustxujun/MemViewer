#pragma once 

#include "CoreMinimal.h"


//#include "LuaSubsystem.h"


class FLuaMemProfiler
{
public:
	static void Initialize(const TCHAR* CmdLine, const TCHAR* Dir);
	static void Start(const TCHAR* Dir);

private:
};

