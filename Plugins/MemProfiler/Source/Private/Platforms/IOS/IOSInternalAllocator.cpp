#if PLATFORM_IOS
#include "IOSInternalAllocator.h"


malloc_zone_t* FIOSIntenralAllocator::InternalZone = 0;

static std::atomic_int64_t TotalSize = 0;


void FIOSIntenralAllocator::Initialize()
{
	InternalZone = malloc_create_zone(vm_kernel_page_size,0);
}

void* FIOSIntenralAllocator::Malloc(size_t Size)
{
	auto Ptr =  malloc_zone_malloc(InternalZone, Size);
	TotalSize += malloc_size(Ptr);
	return Ptr;
}

void FIOSIntenralAllocator::Free(void* Ptr)
{
	TotalSize -= malloc_size(Ptr);
	malloc_zone_free(InternalZone, Ptr);
}


void* FIOSIntenralAllocator::Realloc(void* Ptr, size_t Size)
{
	void* NewPtr = 0;
	if (Size > 0)
		NewPtr= Malloc(Size);
	if (Ptr)
	{
		if (NewPtr)
		{
			auto OldSize = malloc_size(Ptr);
			check(OldSize <= Size);
			FMemory::Memcpy(NewPtr, Ptr, OldSize);
		}
		Free(Ptr);
	}
	return NewPtr;
}


int64 FIOSIntenralAllocator::GetAllocatedSize()
{
	return TotalSize;
}

#endif
