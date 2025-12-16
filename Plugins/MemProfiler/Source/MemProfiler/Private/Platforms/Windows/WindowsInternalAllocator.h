#pragma once 
#include "CoreMinimal.h"
#include "Windows/WindowsHWrapper.h"


class FWindowsInternalAllocator
{
public:
	static void Initialize();
	static void* Malloc(size_t Size);
	static void Free(void* Ptr);
	static void* Realloc(void* Ptr, size_t Size);
	static bool IsInternalPointer(const void* Ptr);
	static int64_t GetAllocatedSize();

	static HANDLE InternalHeap;

};



using FGenericPlatformInternalAllocator = FWindowsInternalAllocator;