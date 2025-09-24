#include "WindowsInternalAllocator.h"
#include "CoreMinimal.h"

HANDLE FWindowsInternalAllocator::InternalHeap = nullptr;

void FWindowsInternalAllocator::Initialize()
{
	InternalHeap = HeapCreate(0, 0, 0);
}


void* FWindowsInternalAllocator::Malloc(size_t Size)
{
	return HeapAlloc(InternalHeap,0,Size);
}
void FWindowsInternalAllocator::Free(void* Ptr)
{
	HeapFree(InternalHeap,0, Ptr);
}

void* FWindowsInternalAllocator::Realloc(void* Ptr, size_t Size)
{
	void* NewPtr = 0;
	if (Size > 0)
		NewPtr= Malloc(Size);
	if (Ptr)
	{
		if (NewPtr)
		{
			auto OldSize = HeapSize(InternalHeap, 0, Ptr);
			check(OldSize <= Size);
			FMemory::Memcpy(NewPtr, Ptr, OldSize);
		}
		Free(Ptr);
	}
	return NewPtr;
}

