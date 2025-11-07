#if PLATFORM_WINDOWS
#include "WinHook.h"
#include "MinHook/include/MinHook.h"
#include "Windows/WindowsHWrapper.h"
#include "WindowsInternalAllocator.h"

static constexpr uint32_t InternalFlag = 1 << 31;

typedef PVOID(WINAPI* RtlAllocateHeap_t)(PVOID _heap, ULONG _flags, SIZE_T _size);
RtlAllocateHeap_t OriginalRtlAllocateHeap;
MallocEvent OnRtlAllocateHeap;
PVOID WINAPI MyRtlAllocateHeap(PVOID hHeap, ULONG _flags, SIZE_T _size)
{
	auto Ptr = OriginalRtlAllocateHeap(hHeap, _flags | InternalFlag, _size);

	if (hHeap == FWindowsInternalAllocator::InternalHeap || Ptr == nullptr || (_flags & InternalFlag) != 0)
		return Ptr;

	OnRtlAllocateHeap(Ptr, FMath::Max<SIZE_T>(_size, HeapSize(hHeap,0,Ptr)), 0);
	return Ptr;
}

typedef BOOLEAN(WINAPI* RtlFreeHeap_t)(PVOID hHeap, ULONG dwFlags, PVOID lpMem);
RtlFreeHeap_t OriginalRtlFreeHeap;
FreeEvent OnRtlFreeHeap;
BOOLEAN WINAPI MyRtlFreeHeap(PVOID hHeap, ULONG dwFlags, PVOID lpMem)
{
	auto Result = OriginalRtlFreeHeap(hHeap, dwFlags | InternalFlag, lpMem);

	if (hHeap == FWindowsInternalAllocator::InternalHeap|| (dwFlags & InternalFlag) != 0)
		return Result;

	OnRtlFreeHeap(lpMem,  0);
	return Result;
}

typedef LPVOID(WINAPI* RtlReAllocateHeap_t)(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes);
RtlReAllocateHeap_t OriginalRtlReAllocateHeap;
LPVOID WINAPI MyRtlReAllocateHeap(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes)
{
	auto Ptr = OriginalRtlReAllocateHeap(hHeap, dwFlags | InternalFlag, lpMem, dwBytes);
	
	if (hHeap == FWindowsInternalAllocator::InternalHeap || Ptr == nullptr|| (dwFlags & InternalFlag) != 0)
		return Ptr;

	OnRtlFreeHeap(lpMem, 0);
	OnRtlAllocateHeap(Ptr, dwBytes, 0);
	return Ptr;
}


typedef LPVOID(WINAPI* VirtualAlloc_t)(LPVOID, SIZE_T, DWORD, DWORD);
VirtualAlloc_t OriginalVirtualAlloc;
MallocEvent OnVirtualAlloc;
LPVOID WINAPI MyVirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
{
	auto Ptr = OriginalVirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
	if ((flAllocationType & MEM_COMMIT) == 0)
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
	auto Result = OriginalVirtualFree(lpAddress, dwSize, dwFreeType);
	if ((dwFreeType & MEM_DECOMMIT) == 0)
	{
		return Result;
	}

	OnVirtualFree(lpAddress, 0);
	return Result;
}


#define CREATE_HOOK(Module, Name, Event) On##Name = Event; MH_CreateHook(::GetProcAddress(Module, #Name), My##Name, (void**)&Original##Name);   

void FWinHook::StartHooking(MallocEvent OnMalloc, FreeEvent OnFree)
{
	MH_Initialize();
	HMODULE hntdll32 = ::GetModuleHandleA("ntdll");
	HMODULE hkernel32 = ::GetModuleHandleA("kernel32");


	// malloc
	CREATE_HOOK(hntdll32,RtlAllocateHeap, OnMalloc);
	CREATE_HOOK(hntdll32, RtlFreeHeap, OnFree);
	MH_CreateHook(::GetProcAddress(hntdll32, "RtlReAllocateHeap"), MyRtlReAllocateHeap, (void**)&OriginalRtlReAllocateHeap);


	// vm
	CREATE_HOOK(hkernel32, VirtualAlloc, OnMalloc);
	CREATE_HOOK(hkernel32, VirtualFree, OnFree);

	MH_EnableHook(MH_ALL_HOOKS);
}


void FWinHook::StopHooking()
{
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}


#endif
