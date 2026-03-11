#if PLATFORM_WINDOWS
#include "WinHook.h"
#include "DynamicTLS.h"
#include "MinHook/include/MinHook.h"
#include "Windows/WindowsHWrapper.h"
#include "WindowsInternalAllocator.h"

struct FHookState
{
	FDynamicTLS<bool> bInternal;
};

FHookState& GetHookState()
{
	static FHookState* State;
	if (State == 0)
	{
		State = new FHookState();
	}

	return *State;
}

static thread_local bool internal_malloc = false;
FORCEINLINE bool IsInternalOpt()
{
	return internal_malloc;
}

struct FScopeInternalOpt
{
	FORCEINLINE FScopeInternalOpt()
	{
		Old = internal_malloc;
		internal_malloc = true;
	}
	FORCEINLINE ~FScopeInternalOpt()
	{
		internal_malloc = Old;
	}
	bool Old;
};

//#define BEGIN_HOOK() FScopeInternalOpt _ScopeOpt;
//#define IsInternalOpt() (_ScopeOpt.Old)

#define BEGIN_HOOK() 
#define IsInternalOpt() false


typedef PVOID(WINAPI* HeapAlloc_t)(PVOID _heap, ULONG _flags, SIZE_T _size);
HeapAlloc_t OriginalHeapAlloc;
MallocEvent OnHeapAlloc;
PVOID WINAPI MyHeapAlloc(PVOID hHeap, ULONG _flags, SIZE_T _size)
{
	BEGIN_HOOK();
	auto Ptr = OriginalHeapAlloc(hHeap, _flags , _size);

	if ( IsInternalOpt() || Ptr == nullptr)
		return Ptr;

	OnHeapAlloc(Ptr, FMath::Max<SIZE_T>(_size, HeapSize(hHeap,0,Ptr)), 0);
	return Ptr;
}


typedef BOOLEAN(WINAPI* HeapFree_t)(PVOID hHeap, ULONG dwFlags, PVOID lpMem);
HeapFree_t OriginalHeapFree;
FreeEvent OnHeapFree;
BOOLEAN WINAPI MyHeapFree(PVOID hHeap, ULONG dwFlags, PVOID lpMem)
{
	BEGIN_HOOK();

	auto Result = OriginalHeapFree(hHeap, dwFlags , lpMem);

	if (IsInternalOpt())
		return Result;

	OnHeapFree(lpMem,  0);
	return Result;
}

typedef LPVOID(WINAPI* HeapRealloc_t)(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes);
HeapRealloc_t OriginalHeapRealloc;
LPVOID WINAPI MyHeapRealloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes)
{
	BEGIN_HOOK();
	auto Ptr = OriginalHeapRealloc(hHeap, dwFlags, lpMem, dwBytes);
	
	if (Ptr == nullptr || IsInternalOpt())
		return Ptr;

	OnHeapFree(lpMem, 0);
	OnHeapAlloc(Ptr, dwBytes, 0);
	return Ptr;
}

typedef LPVOID(WINAPI* VirtualAlloc_t)(LPVOID, SIZE_T, DWORD, DWORD);
VirtualAlloc_t OriginalVirtualAlloc;
MallocEvent OnVirtualAlloc;
LPVOID WINAPI MyVirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
{
	BEGIN_HOOK();
	auto Ptr = OriginalVirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
	if ((flAllocationType & MEM_COMMIT) == 0 || IsInternalOpt())
	{
		return Ptr;
	}

	OnVirtualAlloc(Ptr, dwSize, 0);
	return Ptr;
}


typedef BOOL(WINAPI* VirtualFree_t)(LPVOID, SIZE_T, DWORD);
VirtualFree_t OriginalVirtualFree;
FreeEvent OnVirtualFree;
BOOL WINAPI MyVirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
{
	BEGIN_HOOK();
	auto Result = OriginalVirtualFree(lpAddress, dwSize, dwFreeType);
	if ((dwFreeType & (MEM_DECOMMIT | MEM_RELEASE)) == 0 || IsInternalOpt())
	{
		return Result;
	}

	OnVirtualFree(lpAddress, 0);
	return Result;
}

typedef LONG(* NtAllocateVirtualMemory_t)(HANDLE , PVOID* , ULONG_PTR , PSIZE_T , ULONG , ULONG );
NtAllocateVirtualMemory_t OriginalNtAllocateVirtualMemory;
MallocEvent OnNtAllocateVirtualMemory;
LONG MyNtAllocateVirtualMemory(HANDLE Handle, PVOID *BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect)
{
	auto Result = OriginalNtAllocateVirtualMemory(Handle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
	if ((AllocationType & MEM_COMMIT) == 0 || FWindowsInternalAllocator::IsInternalPointer(*BaseAddress))
	{
		return Result;
	}

	OnNtAllocateVirtualMemory(*BaseAddress, *RegionSize, 0);
	return Result;
}

typedef LONG(* NtFreeVirtualMemory_t)(HANDLE , PVOID* , PSIZE_T , ULONG );
NtFreeVirtualMemory_t OriginalNtFreeVirtualMemory;
FreeEvent OnNtFreeVirtualMemory;
BOOL WINAPI MyNtFreeVirtualMemory(HANDLE Handle, PVOID* BaseAddress,  PSIZE_T RegionSize, ULONG FreeType)
{
	auto Result = OriginalNtFreeVirtualMemory(Handle, BaseAddress, RegionSize, FreeType);
	if ((FreeType & MEM_DECOMMIT) == 0 || FWindowsInternalAllocator::IsInternalPointer(*BaseAddress))
	{
		return Result;
	}

	OnNtFreeVirtualMemory(*BaseAddress, 0);
	return Result;
}



typedef void* (*malloc_t)(size_t);
malloc_t Originalmalloc;
MallocEvent Onmalloc = 0;
void* Mymalloc(size_t Size)
{
	BEGIN_HOOK();
	auto Ptr = Originalmalloc(Size);
	if (IsInternalOpt())
		return Ptr;
	Onmalloc(Ptr, std::max(_msize(Ptr),Size), 0);
	return Ptr;
}


typedef void(*free_t)(void*);
free_t Originalfree;
FreeEvent Onfree;
void Myfree(void* p)
{
	BEGIN_HOOK();
	Originalfree(p);
	if (IsInternalOpt())
		return;
	Onfree(p, 0);
}

typedef void* (*realloc_t)(void*, size_t);
realloc_t Originalrealloc;
LPVOID WINAPI Myrealloc(void* p, size_t Size)
{
	BEGIN_HOOK();
	auto Ptr = Originalrealloc(p, Size);

	if (Ptr == nullptr || IsInternalOpt())
		return Ptr;

	Onfree(p, 0);
	Onmalloc(p, Size, 0);
	return Ptr;
}

typedef void* (*calloc_t)(size_t,size_t);
calloc_t Originalcalloc;
MallocEvent Oncalloc;
void* Mycalloc(size_t Count, size_t Size)
{
	BEGIN_HOOK();
	auto Ptr = Originalcalloc(Count,Size);
	if (IsInternalOpt())
		return Ptr;
	Oncalloc(Ptr,Count * Size, 0);
	return Ptr;
}

typedef void* (*_aligned_malloc_t)(size_t,size_t);
_aligned_malloc_t Original_aligned_malloc;
MallocEvent On_aligned_malloc;
void* My_aligned_malloc(size_t Size,size_t Align)
{
	BEGIN_HOOK();
	auto Ptr = Original_aligned_malloc(Size, Align);
	if (IsInternalOpt())
		return Ptr;

	On_aligned_malloc(Ptr, std::max(_aligned_msize(Ptr, Align, 0), Size), 0);
	return Ptr;
}

typedef void(*_aligned_free_t)(void*);
_aligned_free_t Original_aligned_free;
FreeEvent On_aligned_free;
void My_aligned_free(void* p)
{
	BEGIN_HOOK();
	Original_aligned_free(p);

	if (IsInternalOpt())
		return;

	On_aligned_free(p, 0);
}

typedef void* (*_aligned_realloc_t)(void*, size_t,size_t);
_aligned_realloc_t Original_aligned_realloc;
LPVOID WINAPI My_aligned_realloc(void* p, size_t Size, size_t Align)
{
	BEGIN_HOOK();
	auto Ptr = Original_aligned_realloc(p, Size, Align);

	if (Ptr == nullptr || IsInternalOpt())
		return Ptr;

	On_aligned_free(p, 0);
	On_aligned_malloc(p, std::max(_aligned_msize(Ptr, Align, 0), Size), 0);
	return Ptr;
}

typedef PVOID(WINAPI* RtlAllocateHeap_t)(PVOID _heap, ULONG _flags, SIZE_T _size);
RtlAllocateHeap_t OriginalRtlAllocateHeap = 0;
MallocEvent OnRtlAllocateHeap = 0;
PVOID WINAPI MyRtlAllocateHeap(PVOID hHeap, ULONG _flags, SIZE_T _size)
{
	BEGIN_HOOK();
	auto Ptr = OriginalRtlAllocateHeap(hHeap, _flags, _size);

	if ( IsInternalOpt() || Ptr == nullptr )
		return Ptr;

	OnRtlAllocateHeap(Ptr, FMath::Max<SIZE_T>(_size, HeapSize(hHeap, 0, Ptr)), 0);
	return Ptr;
}


typedef BOOLEAN(WINAPI* RtlFreeHeap_t)(PVOID hHeap, ULONG dwFlags, PVOID lpMem);
RtlFreeHeap_t OriginalRtlFreeHeap;
FreeEvent OnRtlFreeHeap;
BOOLEAN WINAPI MyRtlFreeHeap(PVOID hHeap, ULONG dwFlags, PVOID lpMem)
{
	BEGIN_HOOK();
	auto Result = OriginalRtlFreeHeap(hHeap, dwFlags, lpMem);

	if ( IsInternalOpt())
		return Result;

	OnRtlFreeHeap(lpMem, 0);
	return Result;
}
typedef LPVOID(WINAPI* RtlReAllocateHeap_t)(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes);
RtlReAllocateHeap_t OriginalRtlReAllocateHeap;
LPVOID WINAPI MyRtlReAllocateHeap(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes)
{
	auto Ptr = OriginalRtlReAllocateHeap(hHeap, dwFlags, lpMem, dwBytes);

	if ( Ptr == nullptr )
		return Ptr;

	OnRtlFreeHeap(lpMem, 0);
	OnRtlAllocateHeap(Ptr, dwBytes, 0);
	return Ptr;
}

#define CREATE_HOOK(Module, Name, Event) {auto Addr = ::GetProcAddress(Module, #Name); checkf(Addr, TEXT("invalid addr 0x%x"),(uint64)Addr) On##Name = Event; auto Result = MH_CreateHook(Addr, My##Name, (void**)&Original##Name); checkf(MH_OK == Result, TEXT("hook failed %d"), Result);};   

void FWinHook::StartHooking(MallocEvent OnMalloc, FreeEvent OnFree, bool bVM)
{
	GetHookState();
	IsInternalFunction(0);

	MH_Initialize();
	HMODULE hntdll32 = ::GetModuleHandleA("ntdll");
	HMODULE hkernel32 = ::GetModuleHandleA("kernel32");
	HMODULE hucrtbase = ::GetModuleHandleA("ucrtbase.dll");
	HMODULE hntoskrnl = LoadLibraryA("ntoskrnl.exe");
	HMODULE hwin32u = GetModuleHandleA("win32u.dll");

	auto bCRT = hucrtbase!= 0 && ::GetProcAddress(hucrtbase, "malloc") == 0;

	if (!bVM)
	{
		//if (bCRT)
		//{
		//	Onmalloc = OnMalloc;
		//	Onfree = OnFree;
		//	MH_CreateHook(&malloc, Mymalloc, (void**)&Originalmalloc);
		//	MH_CreateHook(&free, Myfree, (void**)&Originalfree);

		//	MH_CreateHook(&realloc, Myrealloc, (void**)&Originalrealloc);
		//	MH_CreateHook(&_aligned_realloc, My_aligned_realloc, (void**)&Original_aligned_realloc);

		//	On_aligned_malloc = OnMalloc;
		//	On_aligned_free = OnFree;
		//	MH_CreateHook(&_aligned_malloc, My_aligned_malloc, (void**)&Original_aligned_malloc);
		//	MH_CreateHook(&_aligned_free, My_aligned_free, (void**)&Original_aligned_free);

		//	Oncalloc = OnMalloc;
		//	MH_CreateHook(&calloc, Mycalloc, (void**)&Originalcalloc);
		//}
		//else
		//{
		//	CREATE_HOOK(hucrtbase, malloc, OnMalloc);
		//	CREATE_HOOK(hucrtbase, free, OnFree);
		//	CREATE_HOOK(hucrtbase, calloc, OnMalloc);
		//	CREATE_HOOK(hucrtbase, _aligned_malloc, OnMalloc);
		//	CREATE_HOOK(hucrtbase, _aligned_free, OnFree);

		//	MH_CreateHook(::GetProcAddress(hucrtbase, "realloc"), Myrealloc, (void**)&Originalrealloc);
		//	MH_CreateHook(::GetProcAddress(hucrtbase, "_aligned_realloc"), My_aligned_realloc, (void**)&Original_aligned_realloc);
		//}
		// malloc
		//CREATE_HOOK(hkernel32,HeapAlloc, OnMalloc);
		//CREATE_HOOK(hkernel32, HeapFree, OnFree);
		//MH_CreateHook(::GetProcAddress(hkernel32, "HeapRealloc"), MyHeapRealloc, (void**)&OriginalHeapRealloc);

		CREATE_HOOK(hntdll32, RtlAllocateHeap, OnMalloc);
		CREATE_HOOK(hntdll32, RtlFreeHeap, OnFree);
		MH_CreateHook(::GetProcAddress(hntdll32, "RtlReAllocateHeap"), MyRtlReAllocateHeap, (void**)&OriginalRtlReAllocateHeap);
	}
	else
	{
		// vm
		CREATE_HOOK(hntdll32, NtAllocateVirtualMemory, OnMalloc);
		CREATE_HOOK(hntdll32, NtFreeVirtualMemory, OnFree);
	}
	MH_EnableHook(MH_ALL_HOOKS);
}


void FWinHook::StopHooking()
{
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}

bool FWinHook::IsInternalFunction(uint64 Addr)
{
	static struct FindFuncRange
	{
		std::vector<std::pair<uint64, uint64>> ranges;
		FindFuncRange()
		{
			auto AddRange = [&](auto Addr){
				DWORD64 imageBase = 0;
				auto funcEntry = RtlLookupFunctionEntry(
					(DWORD64)Addr,
					&imageBase,
					NULL
				);

				ranges.push_back({(uint64)(imageBase + funcEntry->BeginAddress), (uint64)(imageBase + funcEntry->EndAddress)});
			};

			AddRange(&MyRtlAllocateHeap);
			AddRange(&MyRtlReAllocateHeap);
			//AddRange(&MyRtlFreeHeap);
		}

		bool Match(uint64 Addr)
		{
			for (auto& range : ranges)
			{
				if (range.first <= Addr && Addr <= range.second)
					return true;
			}

			return false;
		}
	} Finder;

	return Finder.Match(Addr);
}

bool FWinHook::IsEnabled()
{
	return OnRtlAllocateHeap != 0 || Onmalloc != 0;
}
#endif
