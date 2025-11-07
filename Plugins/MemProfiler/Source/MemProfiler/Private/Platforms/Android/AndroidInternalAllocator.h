#pragma once 
#include "CoreMinimal.h"


class FAndroidInternalAllocator
{
public:
	static void Initialize();
	static void* Malloc(size_t Size);
	static void Free(void* Ptr);
	static void* Realloc(void* Ptr, size_t Size);

	static int64_t GetAllocatedSize();


};



using FGenericPlatformInternalAllocator = FAndroidInternalAllocator;