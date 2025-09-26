#pragma once 

#include <malloc/malloc.h>


class FIOSIntenralAllocator
{
public:
	static void Initialize();
	static void* Malloc(size_t Size);
	static void Free(void* Ptr);
	
	static void* Realloc(void* Ptr, size_t Size);



	static malloc_zone_t* InternalZone;
};



using FGenericPlatformInternalAllocator = FIOSIntenralAllocator;