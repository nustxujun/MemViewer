#pragma once


#include "CoreMinimal.h"
#include "Serialization/MemoryWriter.h"

#define ENABLE_ALLOCATION_TRACKING !UE_BUILD_SHIPPING

#if ENABLE_ALLOCATION_TRACKING
#define ALLOC_TRACK(x) x
#else
#define ALLOC_TRACK(x)
#endif


enum CustomObjectType
{
	


	COT_Max
};

class FAllocationTracking
{
public:
    static bool IsEnabled();
    static bool IsCapturingStack();
	static bool IsStandalone();
    static int GetTrackerType();
	static uint32 GetCurrentFrame();
    static void Initialize(const TCHAR* CmdLine, const TCHAR* Dir);
    static void TrackAllocation(const void* Ptr, uint32 Size);
    static void TrackFree(const void* Ptr);
	
	static void BeginFrame();
	
	
	static void AddFrameRecorder(const FString& name, TFunction<void(FArchive& Ar)> Recorder);
	
	enum
	{
		Texture,
		TextureArray,
		Texture3D,
		TextureCube,
		TextureCubeArray,
		
		
		Vertex,
		Index,
		Struct,
		Uniform,
	};
	
	static void TrackAllocRHITexture(const void* Ptr, const FString& Name,uint32 Size, uint32 Width, uint32 Height, uint32 Depth, uint32 Format, uint32 NumMips, uint32 SampleCount,uint32 ArrayCount, uint32 Type, uint32 bRT);
	static void TrackFreeRHITexture(const void* Ptr);
	
	static void TrackAllocRHIBuffer(const void* Ptr, const FString& Name, uint32 Size, uint32 Type);
	static void TrackFreeRHIBuffer(const void* Ptr);

	static int GetCurrentStackId();

	constexpr static int CUSTOM_OBJECT_VERSION = 1;

	static void TrackAllocObjectWithData(const void* Ptr, const FString& Name, uint32 Size, uint32 Type, const void* Buffer, uint32 BufferSize);
	static void TrackAllocObject(const void* Ptr, const FString& Name, uint32 Size, uint32 Type);
	static void TrackFreeObject(const void* Ptr);

	template<class ... Args>
	static void TrackAllocObject(const void* Ptr, const FString& Name, uint32 Size,uint32 Type, Args ... Params)
	{
		TArray<uint8> Data;
		Data.Reserve(sizeof...(Args) + sizeof(CUSTOM_OBJECT_VERSION) );
		FMemoryWriter Writer(Data);

		int Ver = CUSTOM_OBJECT_VERSION;
		Writer << Ver;
		int a[] = { ((Writer << Params),0) ...};
		TrackAllocObjectWithData(Ptr, Name, Size, Type, Data.GetData(), Data.Num());
	}

};

