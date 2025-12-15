#pragma once 

#include "CoreMinimal.h"
#include "Hook.h"

class FWinHook
{
public:

	static void StartHooking(MallocEvent OnMalloc, FreeEvent OnFree);
	static void StopHooking();
	static bool IsInternalFunction(uint64 Addr);
	static bool IsEnabled();
};


using FGenericPlatformHook = FWinHook;