#if PLATFORM_WINDOWS
#include "WindowsExternalTracker.h"
#include "AllocationTracking.h"
#include "Windows/WindowsHWrapper.h"
#include "DynamicTLS.h"
#include "WinHook.h"
#include <array>
#include <TraceLoggingProvider.h>
#include <thread>
#include <evntrace.h>
#include <evntcons.h>
#include <atomic>
#include <mutex>
#include "HAL/ThreadManager.h"

#ifndef PLATFORM_SUPPORTS_PLATFORM_EVENTS
	#define PLATFORM_SUPPORTS_PLATFORM_EVENTS 0
#endif

#ifndef UE_TRACE_ENABLED
	#define UE_TRACE_ENABLED 0
#endif

#define UE_ETW_ENABLED  UE_TRACE_ENABLED && PLATFORM_SUPPORTS_PLATFORM_EVENTS

__pragma(warning(disable:4191))
struct FETWUserData
{
	const void* Ptr = 0;
	unsigned int Size;
	unsigned int Frame;
};

static constexpr int MaxNum = 16;
struct FUserDataGroup
{
	std::atomic_uint64_t Total = 0;
	uint64_t Processed = 0;
	REGHANDLE TrackerProvider;
	FUserDataGroup()
	{
		const GUID PROVIDER_GUID = { 0xABCD1234, 0x5678, 0x90AB, { 0xCD, 0xEF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 } };

		EventRegister(&PROVIDER_GUID, NULL, NULL, &TrackerProvider);


		//EventDataDescCreate(&dataDescriptors, (PVOID)eventName, (ULONG)((strlen(eventName) + 1) * sizeof(char)));
	}

	FORCEINLINE void RecordAlloc(const void* Ptr, unsigned int Size, unsigned int Frame)
	{

	}
};

FUserDataGroup& GetUserDataGroup()
{
	static FUserDataGroup* State;
	if (!State)
	{
		State = new FUserDataGroup();
	}
	return *State;
}
void FWindowsExternalTracker::TiggerAlloc(const void* Ptr, unsigned int Size, unsigned int Frame)
{
	EVENT_DESCRIPTOR descriptor = { 0 };
	descriptor.Level = 4;
	descriptor.Id = 1;
	EVENT_DATA_DESCRIPTOR dataDescriptor;
	FETWUserData Data = { Ptr, Size, Frame };
	EventDataDescCreate(&dataDescriptor, &Data, sizeof(Data));
	auto result = EventWrite(GetUserDataGroup().TrackerProvider, &descriptor, 1, &dataDescriptor);
	checkf(result == ERROR_SUCCESS, TEXT("%d"), result);
	GetUserDataGroup().Total.fetch_add(Size, std::memory_order_relaxed);
}


void FWindowsExternalTracker::Start(AllocEvent Alloc, bool bVM)
{
	GetUserDataGroup();

	std::shared_ptr<std::atomic_bool> inited = std::make_shared<std::atomic_bool>(false);
	std::thread t([=]()->void
	{
		WCHAR* sessionName;

		if (!bVM)
			sessionName = L"AllocationTrackingSession";
		else
			sessionName = KERNEL_LOGGER_NAMEW;

		int NameSize = (FCString::Strlen(sessionName) + 1) * sizeof(TCHAR);

#if UE_ETW_ENABLED
		bool skip = false;
		while (!skip)
		{
			FThreadManager::Get().ForEachThread([&](auto, auto Thread) {
				if (Thread->GetThreadName() == TEXT("PlatformEvents"))
				{
					skip = true;
					Thread->Kill();
				}
			});
			Sleep(0);
		}

#endif
		while (true) {
			ULONG size = sizeof(EVENT_TRACE_PROPERTIES) + 1024;
			PEVENT_TRACE_PROPERTIES p = (PEVENT_TRACE_PROPERTIES)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
			if (p) {
				p->Wnode.BufferSize = size;
				p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
				ULONG st = ControlTraceW(0, sessionName, p, EVENT_TRACE_CONTROL_QUERY);
				if (st == ERROR_SUCCESS) {
					ControlTraceW(0, sessionName, p, EVENT_TRACE_CONTROL_STOP);
					HeapFree(GetProcessHeap(), 0, p);
					Sleep(1000);
				}
				else {
					HeapFree(GetProcessHeap(), 0, p);
					break;
				}
			}
		}

		const static bool isTrackingVM = bVM;
		static AllocEvent ProcessFunc = Alloc;
		const static DWORD TargetProcessId = GetCurrentProcessId();

		constexpr static UCHAR VM_ALLOC_OPCODE = 98;
		constexpr static UCHAR VM_FREE_OPCODE = 99;


		static const GUID PROVIDER_GUID = { 0xABCD1234, 0x5678, 0x90AB, { 0xCD, 0xEF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 } };
		static const GUID SystemTraceGuid = { 0x9e814aad, 0x3204, 0x11d2, { 0x9a, 0x82, 0x00, 0x60, 0x08, 0xa8, 0x69, 0x39 } };
		static const GUID PageFaultGuid = { 0x3d6fa8d3, 0xfe05, 0x11d0, { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c } };
		static const GUID StackWalkGuid = { 0xdef2fe46, 0x7bd6, 0x4b80, { 0xbd, 0x94, 0xf5, 0x7f, 0xe2, 0x0d, 0x0c, 0xe3 } };

		//TraceLoggingRegister(TrackerProvider);
		ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + NameSize;
		EVENT_TRACE_PROPERTIES* pSessionProps = (EVENT_TRACE_PROPERTIES*)malloc(bufferSize);
		ZeroMemory(pSessionProps, bufferSize);

		pSessionProps->Wnode.BufferSize = bufferSize;
		pSessionProps->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
		pSessionProps->Wnode.ClientContext = 1;
		pSessionProps->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
		pSessionProps->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

		pSessionProps->BufferSize = 1024 * 2;
		pSessionProps->MinimumBuffers = 1;
		pSessionProps->MaximumBuffers = 4096;
		pSessionProps->FlushTimer = 1;

		if (bVM)
		{
			pSessionProps->Wnode.Guid = PROVIDER_GUID;
			pSessionProps->EnableFlags = EVENT_TRACE_FLAG_VIRTUAL_ALLOC;
		}


		wcscpy_s((WCHAR*)((BYTE*)pSessionProps + pSessionProps->LoggerNameOffset),
			NameSize, sessionName);

		TRACEHANDLE SessionHandle = 0;
		ULONG status = StartTrace(&SessionHandle, sessionName, pSessionProps);
		*inited = true;
		if (status != ERROR_SUCCESS) {
			HRESULT hr = GetLastError();
			printf("ERROR: StartTrace failed with error: %lu\n", status);
			if (status == ERROR_ACCESS_DENIED) {
				printf("ERROR: Access denied. Please run as Administrator!\n");
			}
			free(pSessionProps);
			return;
		}

		status = ControlTrace(SessionHandle, NULL, pSessionProps, EVENT_TRACE_CONTROL_QUERY);

		if (!bVM)
		{
			ENABLE_TRACE_PARAMETERS enableParams;
			ZeroMemory(&enableParams, sizeof(enableParams));
			enableParams.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
			enableParams.EnableProperty = EVENT_ENABLE_PROPERTY_STACK_TRACE;

			status = EnableTraceEx2(
				SessionHandle,
				&PROVIDER_GUID,
				EVENT_CONTROL_CODE_ENABLE_PROVIDER,
				TRACE_LEVEL_VERBOSE,
				0xFFFFFFFFFFFFFFFF,
				0,
				0,
				&enableParams
			);
		}
		else
		{
			CLASSIC_EVENT_ID stackIds[2];
			stackIds[0].EventGuid = PageFaultGuid;
			stackIds[0].Type = VM_ALLOC_OPCODE; // VirtualAlloc
			stackIds[1].EventGuid = PageFaultGuid;
			stackIds[1].Type = VM_FREE_OPCODE; // VirtualFree

			status = TraceSetInformation(SessionHandle, TraceStackTracingInfo, &stackIds, sizeof(stackIds));
		}

		if (status != ERROR_SUCCESS) {
			printf("ERROR: EnableTraceEx2 failed with error: %lu\n", status);
			ControlTrace(SessionHandle, sessionName, pSessionProps, EVENT_TRACE_CONTROL_STOP);
			free(pSessionProps);
			return;
		}

		EVENT_TRACE_LOGFILE traceLogfile;
		ZeroMemory(&traceLogfile, sizeof(EVENT_TRACE_LOGFILE));
		traceLogfile.LoggerName = sessionName;
		traceLogfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
		traceLogfile.EventRecordCallback = +[](PEVENT_RECORD pEvent) {
			auto EventProcessId = pEvent->EventHeader.ProcessId;

			if (isTrackingVM)
			{
				UCHAR Opcode = pEvent->EventHeader.EventDescriptor.Opcode;
				//if (Opcode != VM_ALLOC_OPCODE && Opcode != VM_FREE_OPCODE)
				//	return;

				static FETWUserData LastEvent;
				if (EventProcessId != TargetProcessId)
				{
					if (!IsEqualGUID(pEvent->EventHeader.ProviderId, StackWalkGuid) ||
						LastEvent.Ptr == 0)
						return;

					struct FStackData
					{
						ULONGLONG EventTimeStamp;
						ULONG StackProcess;
						ULONG StackThread;
					};

					auto Count = (pEvent->UserDataLength - sizeof(FStackData)) / sizeof(uint64);
					uint64* Addresses = (uint64*)((uint8*)pEvent->UserData + sizeof(FStackData));

					uint64 Stack[1024];
					int Sum = 0;
					constexpr uint64 Mask = 0xfff00000'00000000;
					int i = 0;
					for (; i < Count; ++i)
					{
						if ((Addresses[i] & Mask) == 0)
							break;
					}

					Stack[0] = Count - i;

					for (int j = 1; i < Count; ++i, ++j)
					{
						Stack[j] = Addresses[i];
					}

					ProcessFunc(LastEvent.Ptr, LastEvent.Size, LastEvent.Frame, Stack);
					LastEvent.Ptr = 0;
				}
				else
				{
					if (!IsEqualGUID(pEvent->EventHeader.ProviderId, PageFaultGuid))
						return;

					struct FVAData
					{
						ULONGLONG BaseAddress;
						ULONGLONG RegionSize;
						ULONG ProcessId;
						ULONG Flags;
					};


					auto Data = (FVAData*)pEvent->UserData;



					if (Opcode == VM_ALLOC_OPCODE)
					{
						if ((Data->Flags & MEM_COMMIT) == 0)
						{
							return;
						}
						LastEvent.Ptr = (const void*)Data->BaseAddress;
						LastEvent.Size = (uint32)Data->RegionSize;
						LastEvent.Frame = FAllocationTracking::GetCurrentFrame();
					}
					else
					{
						if ((Data->Flags & (MEM_DECOMMIT  | MEM_RELEASE)) == 0)
							return;
						ProcessFunc((const void*)Data->BaseAddress, Data->RegionSize, FAllocationTracking::GetCurrentFrame(), nullptr);
					}
				}



			}
			else
			{
				if (EventProcessId != TargetProcessId || !IsEqualGUID(pEvent->EventHeader.ProviderId, PROVIDER_GUID))
					return;


				if (pEvent->ExtendedDataCount == 0) {
					return;
				}

				for (USHORT i = 0; i < pEvent->ExtendedDataCount; i++) {
					EVENT_HEADER_EXTENDED_DATA_ITEM* item = &pEvent->ExtendedData[i];

					if (item->ExtType == EVENT_HEADER_EXT_TYPE_STACK_TRACE64) {
						PEVENT_EXTENDED_ITEM_STACK_TRACE64 pStackTrace = (PEVENT_EXTENDED_ITEM_STACK_TRACE64)item->DataPtr;

						ULONG addressCount = (item->DataSize - sizeof(ULONG64)) / sizeof(ULONG64);
						ULONG64* addresses = pStackTrace->Address;
						unsigned long long Stack[1024];
						const int Skip = 5;
						Stack[0] = std::max<int>((int)addressCount - Skip, 0);

						bool has_internal_func = false;
						for (ULONG j = 0; j < Stack[0]; ++j)
						{
							auto addr = addresses[j];
							if (FWinHook::IsEnabled() && FWinHook::IsInternalFunction(addr))
							{
								if (has_internal_func)
									return;
								else
									has_internal_func = true;
							}
							Stack[j + 1] = addresses[j + Skip];
						}

						FETWUserData* Data = (FETWUserData*)pEvent->UserData;
						ProcessFunc(Data->Ptr, Data->Size, Data->Frame, Stack);
						GetUserDataGroup().Processed++;
						return;
					}
				}
			}

		};


		TRACEHANDLE hTrace = OpenTrace(&traceLogfile);
		if (hTrace == INVALID_PROCESSTRACE_HANDLE) {
			printf("ERROR: OpenTrace failed with error: %lu\n", GetLastError());
			ControlTrace(SessionHandle, sessionName, pSessionProps, EVENT_TRACE_CONTROL_STOP);
			free(pSessionProps);
			return;
		}


		do
		{
			status = ProcessTrace(&hTrace, 1, NULL, NULL);
		} while (status == ERROR_SUCCESS);
		checkf(false, TEXT("profiling has stopped."));

		free(pSessionProps);

	});


#if !UE_ETW_ENABLED
	while (!*inited)
	{
		Sleep(1000);
	}
#endif
	t.detach();
}

#endif
