#pragma once 

#include "CoreMinimal.h"


class FUObjectProfiler
{
public:
    static void Initialize();
	static void Start(const TCHAR* Dir);
	static void UpdateFrame();
};
