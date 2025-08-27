#pragma once 

#include "CoreMinimal.h"


class FMemoryProfiler
{
public:
    static void Initialize(const TCHAR* CmdLine, const TCHAR* Dir);
	static void Start(const TCHAR* Dir);
};
