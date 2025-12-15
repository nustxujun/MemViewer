//#if ENABLE_LOW_LEVEL_MEM_TRACKER

#include "AllocationTracking.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Async/ParallelFor.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Queue.h"
#include "Containers/SortedMap.h"

#include "PlatformCommons.h"
#include "DynamicTLS.h"

#include <map>
#include <atomic>
#include <string>

#if PLATFORM_IOS 
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#elif PLATFORM_ANDROID
#include <sys/mman.h>
#include <dlfcn.h>
#include <link.h>
#include "Android/AndroidPlatformFile.h"
#elif PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

struct AllocInfo
{
	uint64 Ptr;
	uint32 Size;
	uint32 Trace;
	uint32 Begin;
	uint32 Tag;
};

struct DeallocInfo
{
	uint64 Ptr;
	uint32 End;
	uint32 Tag;
};

auto SerializeStr = [](auto& Ar, const FString& Str){
	int StrLen = Str.Len();
	Ar << StrLen;
	Ar.Serialize(TCHAR_TO_ANSI(*Str), StrLen);
};

struct AllocTrackState
{
	FString FolderPath;
	std::atomic_bool bInitialized = false;
	int TrackerType = 1; // ELLMTracker::Default
	uint32 SizeLimit = 0;

	std::atomic_int64_t Overhead = 0;

	FCriticalSection InternalMallocMutex;
	FDynamicTLS<bool> bInternalMalloc;
	bool bCapturingStack = false;
	bool bMallocTrace = true;
	bool bStandAlone = false;
	bool bTrackVM = false;
	bool bHook = true;
	bool bExternalTracker= false;
	bool bPause = false;
};

static AllocTrackState*& GetState()
{
	static AllocTrackState* State = 0;
	return State;
}

static bool IsInternalMalloc()
{
	return GetState()->bInternalMalloc.GetValue();
}

struct FInternalMallocScope
{
	FInternalMallocScope()
	{
		GetState()->bInternalMalloc.SetValue(true);
	}
	~FInternalMallocScope()
	{
		GetState()->bInternalMalloc.SetValue(false);
	}
};

constexpr int CacheSize = 32;
template<class T, bool bAutoDrop = true>
class MappedAlloc
{
public:
	MappedAlloc(const char* path)
	{
		PageSize = FPlatformMemory::GetConstants().PageSize * sizeof(T) * CacheSize;
#if PLATFORM_IOS || PLATFORM_ANDROID
		Handle = open(path, O_RDWR | O_CREAT, S_IRWXU);
		checkf(Handle != -1, TEXT("errno: %d, path: %s"), errno, ANSI_TO_TCHAR(path));
#elif PLATFORM_WINDOWS
		Handle = MakeShareable<FArchive>(IFileManager::Get().CreateFileWriter(ANSI_TO_TCHAR(path), FILEWRITE_AllowRead));

		//FileHandle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		//checkf(FileHandle != INVALID_HANDLE_VALUE, TEXT("errno: %d, path %s"), GetLastError(), ANSI_TO_TCHAR(path));

		//LARGE_INTEGER LI;
		//LI.QuadPart = (NumPages + 1) * PageSize;
		//ensure(SetFilePointerEx(FileHandle, LI, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER);
		//ensure(SetEndOfFile(FileHandle) != 0);
#endif
	}

	~MappedAlloc()
	{

	}
private:
	T* Malloc(uint32 n = 1)
	{
		FScopeLock Lock(&SharedMutex);

		if (Begin + n >= End)
		{
			//FScopeLock InternalLock(&GetState()->InternalMallocMutex);
#if !PLATFORM_WINDOWS
			FInternalMallocScope Scope;
#endif
#if PLATFORM_IOS || PLATFORM_ANDROID

			ensure(ftruncate(Handle, (NumPages + 1) * PageSize) == 0);
			if (bAutoDrop && Buffer)
			{
				//				madvise(Buffer, PageSize, MADV_DONTNEED);
				munmap(Buffer, PageSize);
//				GetState()->Overhead -= PageSize;
			}
			T* Page = (T*)mmap(0, PageSize, PROT_READ | PROT_WRITE, MAP_SHARED, Handle, NumPages * PageSize);
#elif PLATFORM_WINDOWS
			T* Page;




			if (bAutoDrop)
			{
				Page = Buffer;
				if (!Page)
				{
					Page = (T*)FGenericPlatformInternalAllocator::Malloc(PageSize);
				}
			}
			else
			{
				Page = (T*)FGenericPlatformInternalAllocator::Malloc(PageSize);
			}

			//if (Buffer)
			//{
			//	if (!bAutoDrop)
			//	{
			//		FMemory::Memcpy(Buffer, ShadowBuffer, PageSize);
			//	}

			//	UnmapViewOfFile(Buffer);
			//	//GetState()->Overhead -= PageSize;
			//	CloseHandle(Handle);


			//}


			//LARGE_INTEGER LI;
			//LI.QuadPart = (NumPages + 1) * PageSize;
			//ensure(SetFilePointerEx(FileHandle, LI, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER);
			//if (SetEndOfFile(FileHandle) == 0)
			//{
			//	auto Err= GetLastError();
			//	checkf(false, TEXT("errno: %d"), Err);
			//}
			//Handle = CreateFileMappingA(FileHandle, NULL, PAGE_READWRITE, LI.HighPart, LI.LowPart, NULL);
			//checkf(Handle != INVALID_HANDLE_VALUE && Handle != 0, TEXT("errno: %d"), GetLastError());

			//LI.QuadPart = NumPages * PageSize;
			//T* Page = (T*)MapViewOfFile(Handle, FILE_MAP_ALL_ACCESS, LI.HighPart, LI.LowPart, PageSize);
			//checkf(Page, TEXT("errno: %d"), ::GetLastError());
			//if (!bAutoDrop)
			//{
			//	//ShadowBuffer = (T*)malloc(PageSize);
			//	ShadowBuffer = (T*)FGenericPlatformInternalAllocator::Malloc(PageSize);
			//	memset(ShadowBuffer, 0, PageSize);

			//}
#else
			T* Page = 0;
			return nullptr;
#endif
			//GetState()->bInternalMalloc.SetValue(false);

			check(Page);
//			GetState()->Overhead += PageSize;

			NumPages++;

			Buffer = Page;
			Begin = Page;
			End = (T*)((uint8*)Begin + PageSize);

		}
		auto Cur = Begin;
		Begin += n;

		return Cur;
	}
public:
	T* WriteAndGet(const T* Elems, uint32 Size)
	{
		FScopeLock Lock(&SharedMutex);
		auto Data = Malloc(Size);
		FMemory::Memcpy(Data, Elems,  Size * sizeof(T));
#if PLATFORM_WINDOWS
		Handle->Serialize((uint8*)Elems, Size * sizeof(T));
#endif
		return Data;
	}

	void WriteOnly(const T* Elems, uint32 Size)
	{
#if PLATFORM_WINDOWS
		FScopeLock Lock(&SharedMutex);
		Handle->Serialize((uint8*)Elems, Size * sizeof(T));
#else
		FScopeLock Lock(&SharedMutex);
		auto Data = Malloc(Size);
		FMemory::Memcpy(Data, Elems, Size * sizeof(T));
#endif
	}

private:
	T* Buffer = 0;
	T* Begin = 0;
	T* End = 0;
	uint64 PageSize = 0;
	uint32 NumPages = 0;
#if PLATFORM_WINDOWS
	TSharedPtr<FArchive> Handle;
#elif PLATFORM_IOS || PLATFORM_ANDROID
	int Handle = 0;
#endif
	FCriticalSection SharedMutex;
};


struct TraceKey
{
	TraceKey(uint64* InTrace) :Trace(InTrace)
	{
	}

	bool operator < (const TraceKey& Other)const
	{

		auto B1 = Trace;
		auto B2 = Other.Trace;

		auto E1 = Trace + *Trace;
		auto E2 = Other.Trace + *Other.Trace;

		while (B1 != E1 && B2 != E2)
		{
			if (*(B1) == *(B2))
			{
				B1++;
				B2++;
				continue;
			}

			return *B1 < *B2;
		}

		return false;
	}

	int Size()const
	{
		return (int)Trace[0];
	}

	uint64& operator[](int i) const
	{
		return Trace[i + 1];
	}
	uint64* Trace;

	bool operator == (const TraceKey& Other) const
	{
		const auto Count = Trace[0];

		for (uint64 i = 0; i < Count; ++i)
		{
			if (Trace[i] != Other.Trace[i])
				return false;
		}

		return true;
	}
};






uint32 GetTypeHash(const TraceKey& Key)
{
	return FCrc::MemCrc32(Key.Trace + 1, Key.Trace[0] * sizeof(int64));
}


struct AllocTrackEnvironment
{
	MappedAlloc<uint64, false>* MappedStacks = 0;
	MappedAlloc<AllocInfo>* Allocations = 0;
	MappedAlloc<DeallocInfo>* Deallocations = 0;

	TMap<TraceKey, uint32, FDefaultInternalSetAllocator> TraceMap;

	TSharedPtr<FArchive> FrameFile;
	TSharedPtr<FArchive> RHIFile;
	TSharedPtr<FArchive> SnapshotFile;
	TSharedPtr<FArchive> CustomFile;

	//TArray<Recorder> Recorders;

	FCriticalSection TraceMutex;
	FCriticalSection RHIMutex;
	FCriticalSection CustomMutex;
	std::atomic_uint32_t FrameCount = 1;
	std::atomic_uint32_t UnrecordSize = 0;
};

static AllocTrackEnvironment*& GetEnv()
{
	static AllocTrackEnvironment* Env = 0;
	return Env;
}


static bool IsInitialized()
{
	return GetState()->bInitialized;
}

static std::atomic_uint32_t& GetCurrentTime()
{
	return GetEnv()->FrameCount;
}


bool FAllocationTracking::IsEnabled()
{
	return IsInitialized();
}

bool FAllocationTracking::IsCapturingStack()
{
	return GetState()->bCapturingStack;
}

bool FAllocationTracking::IsStandalone()
{
	return GetState()->bStandAlone;
}

uint32 FAllocationTracking::GetCurrentFrame()
{
	return GetCurrentTime();
}
//  void FAllocationTracking::Uninitialize()
//  {
//  #if PLATFORM_IOS
//  	if (bCapturingStack)
//  		malloc_logger = nullptr;
//  #endif
//  	bInitialized = false;
//  }
static FString& GetFolderPath()
{
	return GetState()->FolderPath;
}

FString GetFilePath(const TCHAR* Filename)
{
#if PLATFORM_IOS
	return IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::Combine(GetFolderPath(), Filename));
#elif PLATFORM_ANDROID
	return IAndroidPlatformFile::GetPlatformPhysical().FileRootPath(*FPaths::Combine(GetFolderPath(), Filename));
#else
	return FPaths::Combine(GetFolderPath(), Filename);
#endif
}

FORCENOINLINE static void GetProgramSize()
{
//	auto Stats = FPlatformMemory::GetStats();
//#if PLATFORM_IOS
//	uint32 total_malloc = 0;
//	uint32 total_others = 0;
//	auto dirty_size = GetIOSDirtySize(total_malloc,total_others );
//	int64 Used = Stats.FootPrintMemory - total_others; // ignore memory which is not alloced from malloc
//#else 
//	int64 Used = Stats.UsedPhysical;
//#endif
//	FAllocationTracking::TrackAllocation((void*)1, (uint32)std::max(0,Used), 0);
}

void TrackAllocationImpl(const void* Ptr, uint32 Size, uint32 Frame, uint32 StackId);
int GetCurrentStackId(uint64* Stack);
void FAllocationTracking::Initialize(const TCHAR* CmdLine, const TCHAR* InDir)
{

	if (GetState() != 0)
		return;

	bool bEnabled = FParse::Param(CmdLine, TEXT("memoryprofile")) == true;

	UE_LOG(LogCore, Log, TEXT("allocation tracking is %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));

	if (!bEnabled)
		return;

	FGenericPlatformInternalAllocator::Initialize();

	GetState() = new AllocTrackState();
	GetEnv() = new AllocTrackEnvironment();


	FParse::Value(CmdLine, TEXT("ignoresmallalloc="), GetState()->SizeLimit);
	FParse::Value(CmdLine, TEXT("tracktype="), GetState()->TrackerType);

	GetState()->bMallocTrace = FParse::Param(CmdLine, TEXT("nomalloc")) == false;
	GetState()->bCapturingStack = FParse::Param(CmdLine, TEXT("traceback")) == true;
	GetState()->bHook = FParse::Param(CmdLine, TEXT("hook")) == true;
	GetState()->bTrackVM = FParse::Param(CmdLine, TEXT("trackvm")) == true;
	GetState()->bExternalTracker = FParse::Param(CmdLine, TEXT("externaltracker")) == true;
	GetState()->bPause = FParse::Param(CmdLine, TEXT("pausetracker")) == true;
	auto& FileMgr = IFileManager::Get();
	const FString Dir = InDir;

	auto& FolderPath = GetFolderPath();
	FolderPath = Dir;

	FileMgr.MakeDirectory(*FolderPath, true);


	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();


	auto StackPath = GetFilePath( TEXT("stacks"));
	auto AllocPath = GetFilePath( TEXT("allocations"));
	auto DeallocPath = GetFilePath( TEXT("deallocations"));


	UE_LOG(LogCore, Log, TEXT("%s, %s"), *StackPath, *AllocPath);

	GetEnv()->MappedStacks = new MappedAlloc<uint64, false>(TCHAR_TO_UTF8(*StackPath));
	GetEnv()->Allocations = new MappedAlloc<AllocInfo>(TCHAR_TO_UTF8(*AllocPath));
	GetEnv()->Deallocations = new MappedAlloc<DeallocInfo>(TCHAR_TO_UTF8(*DeallocPath));
	{
		TSharedPtr<FArchive> ModuleFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("modules")), FILEWRITE_AllowRead));

#if PLATFORM_ANDROID
		struct ImageInfo
		{
			std::string name;
			uint64 base;
			uint32 size;

		};
		TArray<ImageInfo> ModuleInfos;
		dl_iterate_phdr(+[](struct dl_phdr_info* Info, size_t /*size*/, void* Data) {
			auto& ModuleInfos = *static_cast<TArray<ImageInfo>*>(Data);
			if (Info->dlpi_name)
			{
				ModuleInfos.Add({Info->dlpi_name, Info->dlpi_addr});
			}
			return 0;
			}, & ModuleInfos);

		int Count = ModuleInfos.Num();
		(*ModuleFile) << Count;
		for (auto& Module : ModuleInfos)
		{
			(*ModuleFile) << Module.base;
			(*ModuleFile) << Module.size;
			int StrLen = Module.name.length();
			(*ModuleFile) << StrLen;
			ModuleFile->Serialize((void*)Module.name.c_str(), StrLen);
		}
#else
		auto Count = FPlatformStackWalk::GetProcessModuleCount();
		TArray< FStackWalkModuleInfo> ModuleInfos;
		ModuleInfos.SetNum(Count);
		Count = FPlatformStackWalk::GetProcessModuleSignatures(ModuleInfos.GetData(), Count);
		if (Count == 0)
		{
			UE_LOG(LogCore, Warning, TEXT("no modules found."));
		}
		(*ModuleFile) << Count;
		for (int i = 0; i < Count; ++i)
		{
			(*ModuleFile) << ModuleInfos[i].BaseOfImage;
			(*ModuleFile) << ModuleInfos[i].ImageSize;
			int StrLen = FCStringWide::Strlen(ModuleInfos[i].ImageName);
			(*ModuleFile) << StrLen;
			ModuleFile->Serialize(TCHAR_TO_ANSI(ModuleInfos[i].ImageName), StrLen);
		}
#endif
	}



	GetEnv()->SnapshotFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("snapshots")), FILEWRITE_AllowRead));
	GetEnv()->FrameFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("frames")), FILEWRITE_AllowRead));
	GetEnv()->RHIFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("rhis")), FILEWRITE_AllowRead));
	GetEnv()->CustomFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("customs")), FILEWRITE_AllowRead));


	if (IsCapturingStack() && GetState()->bMallocTrace)
	{

		if (GetState()->bExternalTracker)
		{
			FGenericPlatformExternalTracker::Start(+[](const void* Ptr, unsigned int Size, unsigned int Frame, unsigned long long* Stack) {
				TrackAllocationImpl(Ptr, Size, Frame, ::GetCurrentStackId(Stack));
				});
		}

		if (GetState()->bHook)
			FGenericPlatformHook::StartHooking(FAllocationTracking::TrackAllocation, FAllocationTracking::TrackFree);
		else
		{
#if ENABLE_MEM_TRACKER
			GMemTrackerAllocEvent = +[](const void* Ptr, SIZE_T Size) {
				FAllocationTracking::TrackAllocation(Ptr, Size, 0);
				};

			GMemTrackerFreeEvent = +[](const void* Ptr) {
				FAllocationTracking::TrackFree(Ptr, 0);
				};
#endif
		}

	}

	GetState()->bInitialized = true;
	GetProgramSize();
};


int FAllocationTracking::GetTrackerType()
{
	return GetState()->TrackerType;
}

#if PLATFORM_WINDOWS
static int FastCaptureBackTraceOnWin64(uint64* BackTrace, uint32 MaxDepth)
{
	CONTEXT ContextRecord;
	RtlCaptureContext(&ContextRecord);

	UINT iFrame;
	UINT nFrames = MaxDepth;
	for (iFrame = 0; iFrame < nFrames; iFrame++)
	{
		DWORD64 ImageBase;
		PRUNTIME_FUNCTION pFunctionEntry = RtlLookupFunctionEntry(ContextRecord.Rip, &ImageBase, NULL);

		if (pFunctionEntry == NULL)
			break;

		PVOID HandlerData;
		DWORD64 EstablisherFrame;
		RtlVirtualUnwind(UNW_FLAG_NHANDLER,
			ImageBase,
			ContextRecord.Rip,
			pFunctionEntry,
			&ContextRecord,
			&HandlerData,
			&EstablisherFrame,
			NULL);

		BackTrace[iFrame] = ContextRecord.Rip;
	}
	return iFrame - 1;
}
#endif

int GetCurrentStackId(uint64* Stack)
{
	constexpr int skip_num = 0;

	if (Stack[0] <= skip_num)
		return -1;

	Stack[0] -= skip_num;
	FMemory::Memmove(Stack + 1, Stack + 1 + skip_num, Stack[0] * sizeof(uint64));

	TraceKey Key(Stack);

	auto& Env = *GetEnv();

	int StackId = 0;

	{
		FScopeLock Lock(&Env.TraceMutex);
		//auto TraceRet = Env.TraceMap.find(Key);
		auto TraceRet = Env.TraceMap.Find(Key);

		static uint32 StackIndex = 0;
		if (TraceRet == nullptr)
		{
			auto TraceCount = Stack[0] + 1;
			auto MappedTrace = Env.MappedStacks->WriteAndGet(Stack, TraceCount);

			Key.Trace = MappedTrace;
			StackId = StackIndex++;
			//Env.TraceMap[Key] = StackId;
#if !PLATFORM_WINDOWS
			FInternalMallocScope Scope;
#endif
			Env.TraceMap.Add(Key, StackId);
		}
		else
		{
			StackId = *TraceRet;
			//StackId = Env.TraceMap.get(TraceRet);
		}
	}

	return StackId;
}

int FAllocationTracking::GetCurrentStackId()
{
	static const int MAX_DEPTH = 1024;
	uint64 Stack[MAX_DEPTH + 1];

#if PLATFORM_WINDOWS 
	Stack[0] = FastCaptureBackTraceOnWin64(Stack + 1, MAX_DEPTH);
#elif PLATFORM_ANDROID
	FInternalMallocScope Scope;
	Stack[0] = CaptureStack(Stack + 1, MAX_DEPTH);
#else
	Stack[0] = FPlatformStackWalk::CaptureStackBackTrace(Stack + 1, MAX_DEPTH);
#endif
	
	return ::GetCurrentStackId(Stack);
}

void TrackAllocationImpl(const void* Ptr, uint32 Size, uint32 Frame, uint32 StackId)
{
	AllocInfo Info;
	Info.Ptr = (uint64)Ptr;
	Info.Size = Size;
	Info.Trace = StackId;
	Info.Begin = Frame;
	//Info.Tag = VMUserTag;
	GetEnv()->Allocations->WriteOnly(&Info, 1);
}

void FAllocationTracking::TrackAllocation(const void* Ptr, uint32 Size, uint32 VMUserTag)
{
	if (Size <= GetState()->SizeLimit || GetState()->bPause)
	{
		GetEnv()->UnrecordSize.fetch_add(Size,std::memory_order_relaxed);
		return;
	}

#if !PLATFORM_WINDOWS
    if (IsInternalMalloc())
        return;
#endif

	if (Ptr == 0)
		return;

	if (GetState()->bExternalTracker)
	{
		FGenericPlatformExternalTracker::TiggerAlloc(Ptr, Size, GetCurrentTime());
	}
	else
	{
		int StackId = GetCurrentStackId();
	
		if (StackId < 0)
			return;

		TrackAllocationImpl(Ptr, Size, GetCurrentTime(),StackId);
	}
}

void FAllocationTracking::TrackFree(const void* Ptr, uint32 VMUserTag)
{
#if !PLATFORM_WINDOWS
    if (IsInternalMalloc())
        return;
#endif
	if (Ptr == 0 || GetState()->bPause)
		return;
	DeallocInfo Info;
	Info.Ptr = (uint64)Ptr;
	Info.End = GetCurrentTime();
	Info.Tag = VMUserTag;
	GetEnv()->Deallocations->WriteOnly(&Info, 1);
}

FCriticalSection& GetRHIMutex()
{

	return GetEnv()->RHIMutex;
}


void FAllocationTracking::TrackAllocRHITexture(const void* Ptr, const FString& Name, uint32 Size, uint32 Width, uint32 Height, uint32 Depth, uint32 Format, uint32 NumMips, uint32 SampleCount, uint32 ArrayCount, uint32 Type, uint32 bRT)
{
	if (!IsInitialized())
		return;

	FScopeLock Lock(&GetRHIMutex());
	auto& RHIFile = GetEnv()->RHIFile;


	uint32 bCreate = 1;
	(*RHIFile) << bCreate;
	uint64 Addr = (uint64)Ptr;
	(*RHIFile) << Addr;
	uint32 Time = GetCurrentTime();
	(*RHIFile) << Time;
	int StrLen = Name.Len();
	(*RHIFile) << StrLen;
	RHIFile->Serialize(TCHAR_TO_ANSI(*Name), StrLen);

	(*RHIFile) << Type;

	(*RHIFile) << Size;

	uint32 SizeX = 1;
	uint32 SizeY = 1;
	uint32 SizeZ = 1;
	switch (Type)
	{

	case TextureArray:
		SizeZ = ArrayCount;
	case Texture:
		SizeX = Width;
		SizeY = Height;
		break;
	case Texture3D:
		SizeX = Width; SizeY = Height; SizeZ = Depth;
		break;
	case TextureCubeArray:
		SizeY = ArrayCount;
	case TextureCube:
		SizeX = Width;
	}

	(*RHIFile) << SizeX;
	(*RHIFile) << SizeY;
	(*RHIFile) << SizeZ;
	(*RHIFile) << Format;
	(*RHIFile) << NumMips;
	(*RHIFile) << bRT;
}

//static const void* P = 0;

void FAllocationTracking::TrackFreeRHITexture(const void* Ptr)
{
	if (!IsInitialized())
		return;
	FScopeLock Lock(&GetRHIMutex());

	auto& RHIFile = GetEnv()->RHIFile;
	//	check (P != Ptr);


	uint32 bCreate = 0;
	(*RHIFile) << bCreate;
	uint64 Addr = (uint64)Ptr;
	(*RHIFile) << Addr;
	uint32 Time = GetCurrentTime();
	(*RHIFile) << Time;
}



void FAllocationTracking::TrackAllocRHIBuffer(const void* Ptr, const FString& Name, uint32 Size, uint32 Type)
{
	if (!IsInitialized())
		return;
	FScopeLock Lock(&GetRHIMutex());

	auto& RHIFile = GetEnv()->RHIFile;

	uint32 bCreate = 1;
	(*RHIFile) << bCreate;
	uint64 Addr = (uint64)Ptr;
	(*RHIFile) << Addr;
	uint32 Time = GetCurrentTime();
	(*RHIFile) << Time;
	int StrLen = Name.Len();
	(*RHIFile) << StrLen;
	RHIFile->Serialize(TCHAR_TO_ANSI(*Name), StrLen);
	(*RHIFile) << Type;

	(*RHIFile) << Size;
}

void FAllocationTracking::TrackFreeRHIBuffer(const void* Ptr)
{
	TrackFreeRHITexture(Ptr);
}

void FAllocationTracking::TrackAllocObject(const void* Ptr, const FString& Name, uint32 Size, uint32 Type)
{
	auto& Custom = *GetEnv()->CustomFile.Get();
	FScopeLock Lock(&GetEnv()->CustomMutex);

	uint32 bCreate = 1;
	Custom << bCreate;
	uint64 Addr = (uint64)Ptr;
	Custom << Addr;
	uint32 Time = GetCurrentTime();
	Custom << Time;

	SerializeStr(Custom, Name);
	Custom << Type; 
	Custom << Size;
	int BufferSize = 0;
	Custom << BufferSize;
}


void FAllocationTracking::TrackAllocObjectWithData(const void* Ptr, const FString& Name, uint32 Size, uint32 Type, const void* Buffer, uint32 BufferSize)
{
	auto& Custom = *GetEnv()->CustomFile.Get();
	FScopeLock Lock(&GetEnv()->CustomMutex);

	uint32 bCreate = 1;
	Custom << bCreate;
	uint64 Addr = (uint64)Ptr;
	Custom << Addr;
	uint32 Time = GetCurrentTime();
	Custom << Time;

	SerializeStr(Custom, Name);
	Custom << Type;
	Custom << Size;
	Custom << BufferSize;
	if (BufferSize > 0)
		Custom.Serialize((void*)Buffer, BufferSize);
	
}
void FAllocationTracking::TrackFreeObject(const void* Ptr)
{
	auto& Custom = *GetEnv()->CustomFile.Get();
	FScopeLock Lock(&GetEnv()->CustomMutex);
	uint32 bCreate = 0;
	Custom << bCreate;
	uint64 Addr = (uint64)Ptr;
	Custom << Addr;
	uint32 Time = GetCurrentTime();
	Custom << Time;
}


void FAllocationTracking::UpdateFrame()
{
	if (!IsInitialized())
		return;

	auto& FrameFile = GetEnv()->FrameFile;

	auto Stats = FPlatformMemory::GetStats();

	uint64 Used = Stats.UsedPhysical;
	//		Used -= (uint64)FMath::Max(0ll,(int64) Overhead);
	uint64 Available = Stats.AvailablePhysical;
	int FrameId = GetCurrentTime();
	(*FrameFile) << FrameId;
	(*FrameFile) << Used;
	(*FrameFile) << Available;
	int64 OH = FGenericPlatformInternalAllocator::GetAllocatedSize();
	(*FrameFile) << OH;



	{
		auto Infos = FGenericPlatformMemoryInfo::GetCustomMemoryInfo();
		int Platform = 0;
		(*FrameFile) << Platform;
		uint32 Count = Infos.size() + 1;
		(*FrameFile) << Count;

		for (auto& info : Infos)
		{
			uint32 len = info.first.length();
			uint32 size = info.second;

			(*FrameFile) << len;
			FrameFile->Serialize((uint8*)info.first.c_str(), len);
			(*FrameFile) << size;
		}

		const static std::string unrecord_str=  "Unrecord";
		int len = unrecord_str.length();
		(*FrameFile) << len;
		FrameFile->Serialize((uint8*)unrecord_str.c_str(), len);
		uint32_t size = GetEnv()->UnrecordSize;
		(*FrameFile) << size;
	}


	GetCurrentTime() += 1;

	FrameFile->Flush();
	{
		FScopeLock Lock(&GetRHIMutex());
		GetEnv()->RHIFile->Flush();
	}
}

static void TakeSnapshot(const FString& Name)
{
	if (!IsInitialized())
		return;
	uint32 Time = GetCurrentTime();
	(*GetEnv()->SnapshotFile) << Time;
	std::string NameStr = TCHAR_TO_UTF8(*Name);
	int Len = NameStr.length();
	(*GetEnv()->SnapshotFile) << Len;
	GetEnv()->SnapshotFile->Serialize((void*)NameStr.c_str(), Len);

	GetEnv()->SnapshotFile->Flush();
}

static FAutoConsoleCommand AllocationTrackingTakeSnpashot(TEXT("alloc.take"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) 
{
	{
		TakeSnapshot(Args.Num() > 0 ? Args[0] : LexToString(GetCurrentTime().load()));
	}
}));

static FAutoConsoleCommand AllocationTrackingEnable(TEXT("alloc.enable"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		{
			GetState()->bPause = false;
		}
}));

static FAutoConsoleCommand AllocationTrackingDisable(TEXT("alloc.disable"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
	{
		GetState()->bPause = true;
	}
}));

//#include <sstream>
//#include <iomanip>
//
//THIRD_PARTY_INCLUDES_START
//#define _NO_CVCONST_H
//#include <DbgHelp.h>
//THIRD_PARTY_INCLUDES_END
//
//
//
//static FAutoConsoleCommand AllocationTrackingGenerateSymbols(TEXT("alloc.gensymbols"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
//{
//
//	auto& Env = *GetEnv();
//
//	
//
//	{
//		auto& FileMgr = IFileManager::Get();
//		{
//			TSharedPtr<FArchive> ModuleFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(GetFolderPath(), TEXT("modules")), FILEWRITE_AllowRead));
//		
//			int32 Count = 1;
//			(*ModuleFile) << Count;
//			uint64 BaseOfImage = 0;
//			(*ModuleFile) << BaseOfImage;
//			uint64 Size = (-1)>> 1;
//
//			(*ModuleFile) << Size;
//
//
//			if (Count == 0)
//			{
//				UE_LOG(LogCore, Warning, TEXT("no modules found."));
//			}
//			(*ModuleFile) << Count;
//			char Name[] = "Total";
//			int Len = FCStringAnsi::Strlen(Name);
//			(*ModuleFile) << Len;
//			ModuleFile->Serialize(Name, Len);
//		}
//	}
//
//	auto Count = FPlatformStackWalk::GetProcessModuleCount();
//	TArray< FStackWalkModuleInfo> ModuleInfos;
//	ModuleInfos.SetNum(Count);
//	Count = FPlatformStackWalk::GetProcessModuleSignatures(ModuleInfos.GetData(), Count);
//
//
//	if (!SymInitialize(GetCurrentProcess(), NULL, false)) {
//		return;
//	}
//
//	static TArray<TPair<ULONG64, std::string>> symbols;
//	symbols.Empty();
//
//	for (int i = 0; i < Count; ++i)
//	{
//		DWORD64 baseAddress = SymLoadModuleEx(
//			GetCurrentProcess(),
//			NULL,
//			TCHAR_TO_ANSI(ModuleInfos[i].ImageName),
//			NULL,
//			0,
//			0,
//			NULL,
//			0
//		);
//
//		if (baseAddress == 0) {
//			HRESULT hr = GetLastError();
//			continue;
//		}
//
//		if (!SymEnumSymbolsEx(GetCurrentProcess(), baseAddress, NULL, +[](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext)->BOOL
//			{
//				DWORD sym_tag = 0;
//
//				SymGetTypeInfo(GetCurrentProcess(), pSymInfo->ModBase, pSymInfo->TypeIndex, TI_GET_SYMTAG, &sym_tag);
//				if (sym_tag == SymTagFunctionType)
//				{
//					symbols.Emplace(pSymInfo->Address, pSymInfo->Name);
//				}
//				else
//				{
//					//std::cout << pSymInfo->Name << std::endl;
//				}
//
//				return true;
//			}, 0, 0)) {
//
//		}
//
//	}
//	SymCleanup(GetCurrentProcess());
//
//	auto File = IFileManager::Get().CreateFileWriter(*FPaths::Combine(GetFolderPath(), TEXT("symbols")), FILEWRITE_AllowRead);
//
//
//	std::stringstream content;
//
//	for (auto& Item : symbols)
//	{
//		//if (LastName != Item.Value)
//		{
//			//LastName = Item.Value;
//			content << Item.Key << " " << Item.Value << std::endl;
//		}
//	}
//
//
//
//	auto Str = content.str();
//
//	File->Serialize((void*)Str.c_str(), Str.length());
//
//	File->Flush();
//	delete File;
//
//}));

