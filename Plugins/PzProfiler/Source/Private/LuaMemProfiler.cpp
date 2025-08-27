#include "LuaMemProfiler.h"
//#include "LuaState.h"
#include "AllocationTracking.h"



struct FStackFrame
{
	std::string Name;
	bool bDiscard = false;
	FStackFrame(FString&& Tex):Name(TCHAR_TO_UTF8(*Tex)){};
};
static TArray<FStackFrame> CallStack;
static int RecordCount = 0;
static FArchive* File = 0;

enum TrackType
{
	TT_PUSH_STACK,
	TT_POP_STACK,
	TT_ALLOC,
	TT_FREE
};

//static void TrackStack()
//{
//	int Diff = RecordCount - (int)CallStack.Num();
//	if (Diff == 0)
//		return;
//
//	if (Diff < 0)
//	{
//		int Type = TT_PUSH_STACK;
//		for (int i = RecordCount; i < CallStack.Num(); ++i)
//		{
//			(*File) << Type;
//			int len = CallStack[i].Name.length();
//			(*File) << len;
//			File->Serialize(CallStack[i].Name.data(), len);
//		}
//	}
//	else
//	{
//		int Type = TT_POP_STACK;
//		for (int i = RecordCount; i > CallStack.Num(); --i)
//		{
//			(*File) << Type;
//		}
//	}
//
//	RecordCount = CallStack.Num();
//}

//static void FuncHooker(lua_State* L, lua_Debug* ar)
//{
//	if (lua_getinfo(L, "nS", ar) == 0)
//	{
//		return;
//	}
//
//
//	switch (ar->event)
//	{
//	case LUA_HOOKTAILCALL:
//		CallStack[CallStack.Num() - 1].bDiscard = true;
//	case LUA_HOOKCALL:
//	{
//		FString Func = FString::Printf(TEXT("%s[%s](%s:%d)"),  UTF8_TO_TCHAR(ar->name?ar->name:""), UTF8_TO_TCHAR(ar->what), *FPaths::GetCleanFilename(ar->source ? FString(ar->source) : FString()), ar->linedefined);
//		CallStack.Emplace(MoveTemp(Func));
//	}
//	break;
//	case LUA_HOOKRET:
//	{
//		do
//		{
//			CallStack.RemoveAt(CallStack.Num() - 1, 1, false);
//		} while (CallStack.Num() != 0 && CallStack[CallStack.Num() - 1].bDiscard);
//
//		TrackStack();
//	}
//	break;
//	default:
//		break;
//	}
//}



static void TrackAlloc(void* Ptr, size_t Size)
{
	int Type = TT_ALLOC;
	(*File) << Type;
	auto P = (uint64)Ptr;
	(*File) << P;
	auto S = (uint32)Size;
	(*File) << S;
	auto T = FAllocationTracking::GetCurrentFrame();
	(*File) << T;
}

//static void TrackFree(void* Ptr)
//{
//	int Type = TT_FREE;
//	(*File) << Type;
//	auto P = (uint64)Ptr;
//	(*File) << P;
//	auto T = FAllocationTracking::GetCurrentFrame();
//	(*File) << T;
//}



//static void* LuaAllocHooker(void* ud, void* ptr, size_t osize, size_t nsize)
//{
//	TrackStack();
//	if (nsize == 0) {
//		TrackFree(ptr);
//		FMemory::Free(ptr);
//		return NULL;
//	}
//	else {
//		if (ptr)
//		{
//			TrackFree(ptr);
//		}
//
//		ptr = FMemory::Realloc(ptr, nsize);
//
//		TrackAlloc(ptr, nsize);
//		return ptr;
//	}
//}

void FLuaMemProfiler::Initialize(const TCHAR* CmdLine, const TCHAR* Dir)
{	
}
void FLuaMemProfiler::Start(const TCHAR* Dir)
{
	//GLuaAllocator = LuaAllocHooker;
	//NS_SLUA::LuaState::onInitEvent.AddLambda([](auto State){
	//	auto L = State->getLuaState();

	//	lua_sethook(L, FuncHooker, LUA_MASKCALL | LUA_MASKRET, 0);

	//});
	//
	//File = IFileManager::Get().CreateFileWriter(*FPaths::Combine(Dir, TEXT("luainfos")));
}


