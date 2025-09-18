#pragma once 
#include "CoreMinimal.h"
#include "Windows/WindowsHWrapper.h"


class FWindowsInternalAllocator
{
public:
	static void Initialize();
	static void* Malloc(size_t Size);
	static void Free(void* Ptr);

	static HANDLE InternalHeap;

};



using FGenericPlatformInternalAllocator = FWindowsInternalAllocator;