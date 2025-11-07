#pragma once 

#include "CoreMinimal.h"
#include "Hook.h"


class FIOSHook
{
public:

	static void StartHooking(MallocEvent OnMalloc, FreeEvent OnFree);
	static void StopHooking();
};


using FGenericPlatformHook = FIOSHook;