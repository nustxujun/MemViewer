#pragma once 

#include "Hook.h"

class FAndroidHook
{
public:

	static void StartHooking(MallocEvent OnMalloc, FreeEvent OnFree);
	static void StopHooking();

	static void* InternalMalloc(size_t Size);
	static void InternalFree(void* Ptr);
};


using FGenericPlatformHook = FAndroidHook;