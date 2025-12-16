#if PLATFORM_WINDOWS
#include "WindowsInternalAllocator.h"
#include "CoreMinimal.h"
#include <mutex>
HANDLE FWindowsInternalAllocator::InternalHeap = nullptr;
std::atomic_int64_t TotalSize = 0;

static constexpr int Alignment = 16;
static constexpr int MaxSmallPoolSize = 4096;

static constexpr int PoolSizes[] = {16,32,64,128,256,512,1024,2048,4096};
static int SizeToIndex[MaxSmallPoolSize + 1] = { 0};
static constexpr auto SmallPoolTotalSize = 1024ull * 1024ull * 1024ull;
constexpr uint64 PoolTotalSize = 16ull * 1024ull * 1024ull * 1024ull;

struct InternalState
{
	uint8* BeginAddress = 0;
	uint8* EndAddress = 0;
	uint8* BigCurAddress = 0;
	void* BigFreeList = 0;
	std::mutex Mutex;
	
	constexpr static int NumPools = sizeof(PoolSizes) / sizeof(int);

	struct Pool
	{
		void* BeginAddress = 0;
		void* EndAddress = 0;
		uint8* CurAddress = 0;
		void* FreeList = 0;
		std::mutex Mutex;
		int Size = 0;
		void* Malloc()
		{
			{
				std::lock_guard<std::mutex> lock(Mutex);
				if (FreeList)
				{ 
					auto Cur = FreeList;
					auto Next = (*(void**)FreeList);
					FreeList = Next;

					if (Cur == EndAddress)
					{

						*(int*)0 = 0;
					}
					return Cur;
				}
				else
				{
					constexpr int PreAlloc = 1024 * 1024 * 1024;
					TotalSize += PreAlloc;
					void* Cur = ::VirtualAlloc(CurAddress, PreAlloc, MEM_COMMIT, PAGE_READWRITE);
					for (int i = 0; i < PreAlloc; i += Size)
					{
						*(void**)Cur = FreeList;
						FreeList = Cur;
						Cur = (uint8*) Cur + Size;

					}
					CurAddress += PreAlloc;
				}
			}
			return Malloc();

		}

		void Free(void* Ptr)
		{
			std::lock_guard<std::mutex> lock(Mutex);
			*(void**)Ptr = FreeList;
			FreeList = Ptr;
		}
	};
	Pool Pools[NumPools] = {0};
};

static InternalState& GetInternalState()
{
	static InternalState* State = 0;
	if (!State)
	{
		State = new InternalState();
	}
	return *State;
}

static size_t GetAllocationSize(void* Ptr)
{
	auto Offset = (uint8*)Ptr - GetInternalState().BeginAddress;
	auto Index = Offset / SmallPoolTotalSize;
	if (Index < InternalState::NumPools)
	{
		return GetInternalState().Pools[Index].Size;
	}
	else
	{
		Ptr = (uint8*)Ptr - 16;
		auto PSize = *((uint64*)Ptr + 1);
		return PSize;
	}
}

bool FWindowsInternalAllocator::IsInternalPointer(const void* Ptr)
{
	return GetInternalState().BeginAddress <= Ptr && Ptr < GetInternalState().EndAddress;
}

static void* InternalMalloc(size_t Size)
{
	auto AlignedSize = Align(Size, Alignment);
	void* Ptr = 0;
	if (AlignedSize < MaxSmallPoolSize)
	{
		auto& Pool = GetInternalState().Pools[SizeToIndex[AlignedSize]];
		Ptr = Pool.Malloc();
	}
	else
	{
		std::lock_guard<std::mutex> lock(GetInternalState().Mutex);
		if (GetInternalState().BigFreeList)
		{
			void** Cur = &GetInternalState().BigFreeList;
			while (*Cur)
			{
				auto Size = *((uint64*)(*Cur) + 1);
				if (AlignedSize <= Size)
				{
					auto Ret = *Cur;
					auto Next = *(void**)(*Cur);
					*Cur = Next;
					return (uint64*)Ret + 2;
				}
				else
				{
					Cur = (void**)*Cur;
				}
			}

		}


		AlignedSize = Align(Size + 16, MaxSmallPoolSize);
		TotalSize += AlignedSize;
		if (GetInternalState().BigCurAddress + AlignedSize >= GetInternalState().EndAddress)
		{
			*(int*)0 = 0;
		}
		auto Cur = (uint64*)VirtualAlloc(GetInternalState().BigCurAddress, AlignedSize, MEM_COMMIT, PAGE_READWRITE);
		GetInternalState().BigCurAddress += AlignedSize;

		*(Cur + 1)= AlignedSize - 16;
		Ptr = Cur + 2;
	}

	if (Ptr >= GetInternalState().BeginAddress && Ptr <= GetInternalState().EndAddress)
	{

	}
	else
	{
		*(int*)0 = 0;
	}
	return Ptr;
}

static void InternalFree(void* Ptr)
{
	auto Offset = (uint8*)Ptr - GetInternalState().BeginAddress;
	auto Index = Offset / SmallPoolTotalSize;
	if (Index < InternalState::NumPools)
	{
		GetInternalState().Pools[Index].Free(Ptr);
	}
	else
	{
		std::lock_guard<std::mutex> lock(GetInternalState().Mutex);
		Ptr = (uint8*)Ptr - 16;
		auto PSize = *((uint64*)Ptr + 1);
		void** Cur = &GetInternalState().BigFreeList;
		while (*Cur)
		{
			auto Size = *((uint64*)(*Cur) + 1);
			if (PSize < Size)
			{
				*(void**)Ptr = *Cur;
				*Cur = Ptr;
				return;
			}
			else
			{
				Cur = (void**)*Cur;
			}
		}

		*(void**)Ptr = 0;
		*Cur = Ptr;
	}
}

void FWindowsInternalAllocator::Initialize()
{
	GetInternalState();

	{
		int idx = 0;
		int val = 16;
		for (int i = 0; i <= MaxSmallPoolSize; i ++)
		{
			if (i >  (val << idx))
			{
				idx++;
			}
			SizeToIndex[i] = idx;

		}
	}

	auto Begin = (uint8*)VirtualAlloc(NULL, PoolTotalSize, MEM_RESERVE | MEM_TOP_DOWN, PAGE_NOACCESS);
	GetInternalState().BeginAddress = Begin;
	GetInternalState().EndAddress = Begin + PoolTotalSize;
	GetInternalState().BigCurAddress = Begin;
	int Size = 16;
	for (auto& Pool : GetInternalState().Pools)
	{
		Pool.BeginAddress = Begin;
		Pool.EndAddress = Begin + SmallPoolTotalSize;
		Pool.CurAddress = Begin;
		Pool.Size = Size;
		Size <<= 1;
		Begin += SmallPoolTotalSize;
		GetInternalState().BigCurAddress += SmallPoolTotalSize;
	}
}

void* FWindowsInternalAllocator::Malloc(size_t Size)
{
	return InternalMalloc(Size);
}
void FWindowsInternalAllocator::Free(void* Ptr)
{
	InternalFree(Ptr);
}

void* FWindowsInternalAllocator::Realloc(void* Ptr, size_t Size)
{

	void* NewPtr = 0;
	if (Ptr)
	{
		if (Size > 0)
		{
			auto OldSize = GetAllocationSize(Ptr);
			if (Size <= OldSize)
			{
				return Ptr;
			}
			else
			{
				NewPtr = Malloc(Size);
				FMemory::Memcpy(NewPtr, Ptr, OldSize);
			}
		}

		Free(Ptr);
	}
	else if (Size > 0)
	{
		NewPtr = Malloc(Size);
	}
	return NewPtr;
}

int64_t FWindowsInternalAllocator::GetAllocatedSize()
{
	return TotalSize;
}
#endif