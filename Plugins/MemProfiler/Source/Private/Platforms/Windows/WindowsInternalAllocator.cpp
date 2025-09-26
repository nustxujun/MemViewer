#if PLATFORM_WINDOWS
#include "WindowsInternalAllocator.h"
#include "CoreMinimal.h"

HANDLE FWindowsInternalAllocator::InternalHeap = nullptr;
std::atomic_int64_t TotalSize = 0;

void FWindowsInternalAllocator::Initialize()
{
	InternalHeap = HeapCreate(0, 0, 0);
}


void* FWindowsInternalAllocator::Malloc(size_t Size)
{
	auto Ptr = HeapAlloc(InternalHeap,0,Size);
	TotalSize += HeapSize(InternalHeap, 0, Ptr);
	return Ptr;
}
void FWindowsInternalAllocator::Free(void* Ptr)
{
	TotalSize -= HeapSize(InternalHeap, 0, Ptr);
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

int64_t FWindowsInternalAllocator::GetAllocatedSize()
{
	return TotalSize;
}
#endif