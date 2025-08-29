#include "ObjectProfiler.h"
#include <atomic>
#include "Containers/Queue.h"
#include "GenericPlatform/GenericPlatformFile.h"

#include "AllocationTracking.h"
#include "Containers/Queue.h"

#include "Engine/Texture2D.h"


static void WriteString(FArchive& ObjectFile, const FString& Str){
    int Len = Str.Len();
    ObjectFile << Len;
    ObjectFile.Serialize(TCHAR_TO_ANSI(*Str), Len);
};


static TMap<FName,TFunction<void(FArchive&, UObject*)> > Exporters = 
{
	//{TEXT("Texture2D"), [](FArchive& Ar, UObject* Obj){
	//	auto Tex = Cast<UTexture2D>(Obj);
 //       check(Tex);
	//	bool bStreaming = Tex->RenderResourceSupportsStreaming();
	//	auto DroppedMips = Tex->GetNumMips() - Tex->GetNumResidentMips();
	//	auto CurWidth = FMath::Max<int32>(Tex->GetSizeX() >> DroppedMips, 1);
	//	auto CurHeight = FMath::Max<int32>(Tex->GetSizeY() >> DroppedMips, 1);
	//	auto Size = Tex->CalcTextureMemorySizeEnum(TMC_ResidentMips);
	//	auto Format = GetPixelFormatString(Tex->GetPixelFormat());
	//    WriteString(Ar,FString::Printf(TEXT("streaming:%d\nwidth:%d\nheight:%d\nsize:%d\nformat:%s"), (int)bStreaming , CurWidth, CurHeight, Size, Format ));
	//	
	//}},
	//{TEXT("VtaSlateTexture"), [](FArchive& Ar, UObject* Obj){
	//	auto Tex = Cast<UVtaSlateTexture>(Obj);
 //       check(Tex);
	//	FString AtlasName;
 //       if (Tex->AtlasTexture)
 //           AtlasName = Tex->AtlasTexture->GetFullName();
	//	auto Size = Tex->GetDimensions();
	//	WriteString(Ar, FString::Printf(TEXT("atlas:%s\nwidth:%d\nheight:%d"),*AtlasName, (int32)Size.X, (int32)Size.Y));;
	//}}
};

static void ExportExtraInfo(FArchive& Ar, UObject* Obj)
{
    auto Class = Obj->GetClass();
    check(Class);
	auto Exporter = Exporters.Find(Class->GetFName());
	int HasExporter = Exporter != nullptr;
	Ar << HasExporter;
	
	if (HasExporter)
		(*Exporter)(Ar, Obj);
}

struct FUObjectListener: public FUObjectArray::FUObjectCreateListener, public FUObjectArray::FUObjectDeleteListener
{
	struct FObjectInfo
	{
		uint64 Ptr;
		FString Name;
		int ClassIdx;
		uint32_t Time;
		int StackId;
	};
	
	struct FClassInfo
	{
		TArray<FString> ClassChains;
		int ClassIdx;
	};
	
	bool bInit = false;
	
	FUObjectListener()
	{

		GUObjectArray.AddUObjectCreateListener(this);
		GUObjectArray.AddUObjectDeleteListener(this);
		
		FAllocationTracking::AddFrameRecorder(TEXT("objects"), [this](FArchive& Ar){
			Process(Ar);

		});
				
		
	}

	void NotifyUObjectCreated(const UObjectBase* Object, int32 Index) override
	{
		auto Class = Object->GetClass();
		if (Class == nullptr)
			return;
		FScopeLock Lock(&Mutex);

		int ClassIndex = 0;
		auto Ret = ClassMap.Find(*Class->GetPathName());
		if (!Ret)
		{
			FClassInfo Info;
			Info.ClassIdx = ClassMap.Num();
			ClassIndex = Info.ClassIdx;
			auto CurClass = Class;
			while(CurClass)
			{
				Info.ClassChains.Add(CurClass->GetName());
				CurClass = CurClass->GetSuperClass();
			}
			
			ClassMap.Add(FName(Class->GetPathName()), ClassIndex);
			ClassList.Add(MoveTemp(Info));
		}
		else
		{
			ClassIndex = *Ret;
		}
		
		
		ObjectList.Add(FObjectInfo{(uint64)Object, ((UObject*)Object)->GetPathName(), ClassIndex,FAllocationTracking::GetCurrentFrame(), FAllocationTracking::GetCurrentStackId()});

	}  
	
	void NotifyUObjectDeleted(const UObjectBase* Object, int32 Index) override
	{
		auto Class = Object->GetClass();
		if (Class == nullptr)
			return;
		FScopeLock Lock(&Mutex);
		ObjectList.Add(FObjectInfo{(uint64)Object,{},{}, FAllocationTracking::GetCurrentFrame(), 0});
	}

	void OnUObjectArrayShutdown() override
	{
		
	}

	void Process(FArchive& ObjectFile)
	{
		TArray<FObjectInfo> Objects;
		TArray<FClassInfo> Class;
		
		
		{
			FScopeLock Lock(&Mutex);
			Objects = MoveTemp(ObjectList);
			Class = MoveTemp( ClassList);
			
			ObjectList.Reserve(16);
			ClassList.Reserve(16);
		}
		
		
		for (auto& Info : Class)
		{
			int State = 0; // class
			ObjectFile << State;
			int Num = Info.ClassChains.Num();
			ObjectFile << Num;
			for (auto& Name: Info.ClassChains)
			{
				WriteString(ObjectFile, Name);
			}
		}
		
		for (auto& Info : Objects)
		{
			int State = Info.StackId == 0? 1 : 2;
			ObjectFile << State;
			ObjectFile << Info.Ptr;
			ObjectFile << Info.Time;
			
			if (State == 2)
			{	
				ObjectFile << Info.ClassIdx;
				ObjectFile << Info.StackId;
				WriteString(ObjectFile,Info.Name);
			}
		}
		

		ObjectFile.Flush();
	}


	TArray<FObjectInfo> ObjectList;
	TArray<FClassInfo> ClassList;
	TMap<FName, int> ClassMap;
	FCriticalSection Mutex;

};




void FUObjectProfiler::Initialize()
{
}

void FUObjectProfiler::Start()
{
	bool bEnabled = FParse::Param(FCommandLine::Get(), TEXT("trackuobj")) == true;
	if (!bEnabled)
			return;
	static FUObjectListener Listener;

}
