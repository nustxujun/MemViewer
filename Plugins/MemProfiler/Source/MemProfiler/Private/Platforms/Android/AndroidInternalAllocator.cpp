#if PLATFORM_ANDROID
#include "AndroidInternalAllocator.h"
#include "CoreMinimal.h"
#include "AndroidHook.h"
#include <malloc.h>

std::atomic_int64_t TotalSize = 0;

typedef void* (*malloc_t)(size_t size);
typedef void (*free_t)(void*);

static malloc_t InternalMalloc;
static free_t InternalFree;
PRAGMA_DISABLE_OPTIMIZATION
void FAndroidInternalAllocator::Initialize()
{
	//FAndroidHook::GetInternalMethod((void**) & InternalMalloc, (void**) & InternalFree);
}


void* FAndroidInternalAllocator::Malloc(size_t Size)
{
	TotalSize += Size;
	return FAndroidHook::InternalMalloc(Size);
}
void FAndroidInternalAllocator::Free(void* Ptr)
{
	TotalSize -= malloc_usable_size(Ptr);
	FAndroidHook::InternalFree(Ptr);
}

void* FAndroidInternalAllocator::Realloc(void* Ptr, size_t Size)
{
	void* NewPtr = 0;
	if (Size > 0)
		NewPtr = Malloc(Size);
	if (Ptr)
	{
		if (NewPtr)
		{
			auto OldSize = malloc_usable_size(Ptr);
			check(OldSize <= Size);
			FMemory::Memcpy(NewPtr, Ptr, OldSize);
		}
		Free(Ptr);
	}
	return NewPtr;
}

int64_t FAndroidInternalAllocator::GetAllocatedSize()
{
	return TotalSize;
}

PRAGMA_ENABLE_OPTIMIZATION
#endif