#pragma once 

#include "CoreMinimal.h"
#include "Hook.h"


class FIOSHook
{
public:

	static void StartHooking(MallocEvent OnMalloc, FreeEvent OnFree);
};


using FGenericPlatformHook = FIOSHook;