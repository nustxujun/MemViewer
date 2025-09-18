#if PLATFORM_WINDOWS
#include "WinHook.h"
#include "MinHook/include/MinHook.h"
#include "Windows/WindowsHWrapper.h"
#include "WindowsInternalAllocator.h"


typedef PVOID(WINAPI* RtlAllocateHeap_t)(PVOID _heap, ULONG _flags, SIZE_T _size);
RtlAllocateHeap_t OriginalRtlAllocateHeap;
MallocEvent OnRtlAllocateHeap;
PVOID WINAPI RtlAllocateHeap(PVOID hHeap, ULONG _flags, SIZE_T _size)
{
	auto Ptr = OriginalRtlAllocateHeap(hHeap, _flags, _size);

	if (hHeap == FWindowsInternalAllocator::InternalHeap)
		return Ptr;

	OnRtlAllocateHeap(Ptr, _size, 0);
	return Ptr;
}

typedef BOOLEAN(WINAPI* RtlFreeHeap_t)(PVOID hHeap, ULONG dwFlags, PVOID lpMem);
RtlFreeHeap_t OriginalRtlFreeHeap;
FreeEvent OnRtlFreeHeap;
BOOLEAN WINAPI RtlFreeHeap(PVOID hHeap, ULONG dwFlags, PVOID lpMem)
{
	auto Result = OriginalRtlFreeHeap(hHeap, dwFlags, lpMem);

	if (hHeap == FWindowsInternalAllocator::InternalHeap)
		return Result;

	OnRtlFreeHeap(lpMem,  0);
	return Result;
}

typedef LPVOID(WINAPI* RtlReAllocateHeap_t)(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes);
RtlReAllocateHeap_t OriginalRtlReAllocateHeap;
LPVOID WINAPI RtlReAllocateHeap(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes)
{
	auto Ptr = OriginalRtlReAllocateHeap(hHeap, dwFlags, lpMem, dwBytes);
	
	OnRtlFreeHeap(lpMem, 0);
	OnRtlAllocateHeap(Ptr, dwBytes, 0);
	return Ptr;
}


#define CREATE_HOOK(Name, Event) On##Name = Event; MH_CreateHook(::GetProcAddress(hntdll32, #Name), Name, (void**)&Original##Name);   

void FWinHook::StartHooking(MallocEvent OnMalloc, FreeEvent OnFree)
{
	MH_Initialize();
	HMODULE hntdll32 = ::GetModuleHandleA("ntdll");

	CREATE_HOOK(RtlAllocateHeap, OnMalloc);
	CREATE_HOOK(RtlFreeHeap, OnFree);

	MH_CreateHook(::GetProcAddress(hntdll32, "RtlReAllocateHeap"), RtlReAllocateHeap, (void**)&OriginalRtlReAllocateHeap);

	MH_EnableHook(MH_ALL_HOOKS);
}


#endif
