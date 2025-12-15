#if PLATFORM_WINDOWS
#include "WindowsExternalTracker.h"
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
__pragma(warning(disable:4191))

//TRACELOGGING_DEFINE_PROVIDER(
//	TrackerProvider,
//	"PureEtwStackProvider",
//	(0xABCD1234, 0x5678, 0x90AB, 0xCD, 0xEF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66)
//);

struct FETWUserData
{
	const void* Ptr;
	unsigned int Size;
	unsigned int Frame;
};

static constexpr int MaxNum = 16;
struct FUserDataGroup
{
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
}

void FWindowsExternalTracker::Start(AllocEvent Alloc)
{
	GetUserDataGroup();

	static AllocEvent ProcessFunc = Alloc;
	static const GUID PROVIDER_GUID = { 0xABCD1234, 0x5678, 0x90AB, { 0xCD, 0xEF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 } };


	//TraceLoggingRegister(TrackerProvider);

	WCHAR sessionName[] = L"PureEtwStackSession";
	ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(sessionName);
	EVENT_TRACE_PROPERTIES* pSessionProps = (EVENT_TRACE_PROPERTIES*)malloc(bufferSize);
	ZeroMemory(pSessionProps, bufferSize);

	pSessionProps->Wnode.BufferSize = bufferSize;
	pSessionProps->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
	pSessionProps->Wnode.ClientContext = 1;
	//pSessionProps->Wnode.Guid = PROVIDER_GUID;
	pSessionProps->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
	pSessionProps->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);


	wcscpy_s((WCHAR*)((BYTE*)pSessionProps + pSessionProps->LoggerNameOffset),
		sizeof(sessionName) / sizeof(WCHAR), sessionName);


	ControlTrace(0, sessionName, pSessionProps, EVENT_TRACE_CONTROL_STOP);

	pSessionProps->BufferSize = 1024 * 2;
	pSessionProps->MinimumBuffers = 1;
	pSessionProps->MaximumBuffers = 4096;
	pSessionProps->FlushTimer = 1;

	TRACEHANDLE SessionHandle = 0;
	ULONG status = StartTrace(&SessionHandle, sessionName, pSessionProps);
	if (status != ERROR_SUCCESS) {
		printf("ERROR: StartTrace failed with error: %lu\n", status);
		if (status == ERROR_ACCESS_DENIED) {
			printf("ERROR: Access denied. Please run as Administrator!\n");
		}
		free(pSessionProps);
		return ;
	}

	status = ControlTrace(SessionHandle, NULL, pSessionProps, EVENT_TRACE_CONTROL_QUERY);

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

	if (status != ERROR_SUCCESS) {
		printf("ERROR: EnableTraceEx2 failed with error: %lu\n", status);
		ControlTrace(SessionHandle, sessionName, pSessionProps, EVENT_TRACE_CONTROL_STOP);
		free(pSessionProps);
		return ;
	}

	EVENT_TRACE_LOGFILE traceLogfile;
	ZeroMemory(&traceLogfile, sizeof(EVENT_TRACE_LOGFILE));
	traceLogfile.LoggerName = sessionName;
	traceLogfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
	traceLogfile.EventRecordCallback = +[](PEVENT_RECORD pEvent){
		if (!IsEqualGUID(pEvent->EventHeader.ProviderId, PROVIDER_GUID)) {
			return;
		}

		if (pEvent->ExtendedDataCount == 0) {
			return ;
		}

		for (USHORT i = 0; i < pEvent->ExtendedDataCount; i++) {
			EVENT_HEADER_EXTENDED_DATA_ITEM* item = &pEvent->ExtendedData[i];

			if (item->ExtType == EVENT_HEADER_EXT_TYPE_STACK_TRACE64) {
				PEVENT_EXTENDED_ITEM_STACK_TRACE64 pStackTrace = (PEVENT_EXTENDED_ITEM_STACK_TRACE64)item->DataPtr;

				ULONG addressCount = (item->DataSize - sizeof(ULONG64)) / sizeof(ULONG64);
				ULONG64* addresses = pStackTrace->Address;
				unsigned long long Stack[1024];
				Stack[0] = addressCount;

				bool has_internal_func = false;
				for (ULONG j = 0; j < addressCount; ++j)
				{
					auto addr = addresses[j];
					if (FWinHook::IsEnabled() && FWinHook::IsInternalFunction(addr))
					{
						if (has_internal_func)
							return;
						else
							has_internal_func = true;
					}
					Stack[j + 1] = addresses[j];
				}

				FETWUserData* Data = (FETWUserData*)pEvent->UserData;
				ProcessFunc(Data->Ptr, Data->Size, Data->Frame, Stack);
				return ;
			}
		}
	};


	TRACEHANDLE hTrace = OpenTrace(&traceLogfile);
	if (hTrace == INVALID_PROCESSTRACE_HANDLE) {
		printf("ERROR: OpenTrace failed with error: %lu\n", GetLastError());
		ControlTrace(SessionHandle, sessionName, pSessionProps, EVENT_TRACE_CONTROL_STOP);
		free(pSessionProps);
		return ;
	}


	std::thread thread([=]() mutable
	{
		do
		{
			status = ProcessTrace(&hTrace, 1, NULL, NULL);
		}
		while(status == ERROR_SUCCESS);
		checkf(false, TEXT("profiling has stopped."));

		//TraceLoggingUnregister(TrackerProvider);

	});

	thread.detach();
	free(pSessionProps);
}

#endif
