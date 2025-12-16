#pragma once 

#include "Hook.h"


class FWindowsExternalTracker
{
public :
	typedef void (*AllocEvent)(const void* Ptr, unsigned int Size, unsigned int Frame, unsigned long long * Stacks);

	static void TiggerAlloc(const void* Ptr, unsigned int Size, unsigned int Frame);
	static void Start(AllocEvent Alloc, bool bVM);
};


using FGenericPlatformExternalTracker = FWindowsExternalTracker;