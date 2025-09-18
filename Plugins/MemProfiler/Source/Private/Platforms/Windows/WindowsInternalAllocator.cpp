#pragma once 
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

