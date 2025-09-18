#pragma once 

#include "CoreMinimal.h"
#include "Hook.h"

class FWinHook
{
public:

	static void StartHooking(MallocEvent OnMalloc, FreeEvent OnFree);
};


using FGenericPlatformHook = FWinHook;