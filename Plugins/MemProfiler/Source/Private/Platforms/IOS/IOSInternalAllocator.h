#pragma once 

#include <malloc/malloc.h>


class FIOSIntenralAllocator
{
public:
	static void Initialize();
	static void* Malloc(size_t Size);
	static void Free(void* Ptr);

	static malloc_zone_t* InternalZone;
};



using FGenericPlatformInternalAllocator = FIOSIntenralAllocator;