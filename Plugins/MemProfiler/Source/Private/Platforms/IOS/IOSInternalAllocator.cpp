#if PLATFORM_IOS
#include "IOSInternalAllocator.h"

malloc_zone_t* FIOSIntenralAllocator::InternalZone = 0;

void FIOSIntenralAllocator::Initialize()
{
	InternalZone = malloc_create_zone(vm_kernel_page_size,0);
}

void* FIOSIntenralAllocator::Malloc(size_t Size)
{
	return malloc_zone_malloc(InternalZone, Size);
}

void FIOSIntenralAllocator::Free(void* Ptr)
{
	malloc_zone_free(InternalZone, Ptr);
}
#endif