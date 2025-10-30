#include "LuaMemProfiler.h"
#include "AllocationTracking.h"



struct FStackFrame
{
	std::string Name;
	bool bDiscard = false;
	FStackFrame(const TCHAR* Ctnt):Name(TCHAR_TO_UTF8(Ctnt)){};
};

struct FCallstack
{
	TArray<FStackFrame> CallStack;
	int RecordCount = 0;
};

static TMap<const void*, FCallstack > Threads;
static const void* CurrentThread = 0;
static FArchive*& GetFile()
{
	static FArchive* File = 0;
	return File;
}

enum TrackType
{
	TT_PUSH_STACK,
	TT_POP_STACK,
	TT_ALLOC,
	TT_FREE
};

static void TrackStack(const void* ThreadId)
{
	auto& Thread = Threads.FindOrAdd(ThreadId);
	auto& CallStack = Thread.CallStack;
	auto& RecordCount = Thread.RecordCount;
	int Diff = RecordCount - (int)CallStack.Num();
	if (Diff == 0)
		return;

	uint64 Id = (uint64)ThreadId;
	if (Diff < 0)
	{
		int Count = CallStack.Num() - RecordCount;
		if (Count > 0)
		{
			int Type = TT_PUSH_STACK;
			(*GetFile()) << Type;
			(*GetFile()) << Id;
			(*GetFile()) << Count;

			for (int i = RecordCount; i < CallStack.Num(); ++i)
			{
				int len = CallStack[i].Name.length();
				(*GetFile()) << len;
				GetFile()->Serialize(CallStack[i].Name.data(), len);
			}
		}
	}
	else
	{
		int Count = RecordCount - CallStack.Num();
		if (Count > 0)
		{
			int Type = TT_POP_STACK;
			(*GetFile()) << Type;
			(*GetFile()) << Id;
			(*GetFile()) << Count;
		}
	}

	RecordCount = CallStack.Num();
}



#define LUA_HOOKCALL	0
#define LUA_HOOKRET	1
#define LUA_HOOKLINE	2
#define LUA_HOOKCOUNT	3
#define LUA_HOOKTAILCALL 4

void FLuaMemProfiler::CallEvent(const struct lua_State* Thread, int Event, const TCHAR* Content)
{
	CurrentThread = Thread;
	auto& CallStack = Threads.FindOrAdd(Thread).CallStack;
	switch (Event)
	{
	case LUA_HOOKTAILCALL:
		CallStack[CallStack.Num() - 1].bDiscard = true;
	case LUA_HOOKCALL:
	{
		CallStack.Emplace(Content);
	}
	break;
	case LUA_HOOKRET:
	{
		do
		{
			CallStack.RemoveAt(CallStack.Num() - 1, 1, false);
		} while (CallStack.Num() != 0 && CallStack[CallStack.Num() - 1].bDiscard);
	
		TrackStack(Thread);
	}
	break;
	default:
		break;
	}
}


static void TrackAlloc(void* Ptr, size_t Size)
{
	int Type = TT_ALLOC;
	(*GetFile()) << Type;
	auto P = (uint64)Ptr;
	(*GetFile()) << P;
	auto S = (uint32)Size;
	(*GetFile()) << S;
	auto T = FAllocationTracking::GetCurrentFrame();
	(*GetFile()) << T;
}

static void TrackFree(void* Ptr)
{
	int Type = TT_FREE;
	(*GetFile()) << Type;
	auto P = (uint64)Ptr;
	(*GetFile()) << P;
	auto T = FAllocationTracking::GetCurrentFrame();
	(*GetFile()) << T;
}



void* FLuaMemProfiler::Allocator(void* ud, void* ptr, size_t osize, size_t nsize)
{
	if (CurrentThread)
		TrackStack(CurrentThread);

	if (nsize == 0) {
		TrackFree(ptr);
		FMemory::Free(ptr);
		return NULL;
	}
	else {
		if (ptr)
		{
			TrackFree(ptr);
		}

		ptr = FMemory::Realloc(ptr, nsize);

		TrackAlloc(ptr, nsize);
		return ptr;
	}
}

void FLuaMemProfiler::Initialize(const TCHAR* CmdLine, const TCHAR* Dir)
{	
	bool bEnabled = FParse::Param(CmdLine, TEXT("tracklua")) == true;
	if (bEnabled)
		GetFile() = IFileManager::Get().CreateFileWriter(*FPaths::Combine(Dir, TEXT("luainfos")));
}
void FLuaMemProfiler::Start(const TCHAR* Dir)
{

}


bool FLuaMemProfiler::IsEnabled()
{
	return GetFile() != 0;
}