#include "MemoryProfiler.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "AllocationTracking.h"
#include <string>

#include "RHIResources.h"

void FMemoryProfiler::Initialize(const TCHAR* CmdLine, const TCHAR* Dir)
{

	FAllocationTracking::Initialize(CmdLine, Dir);
}



void FMemoryProfiler::Start(const TCHAR* Dir)
{
	FCoreDelegates::OnPostEngineInit.AddLambda([](){
		GEngine->Exec(nullptr, TEXT("rhi.Metal.ForceIOSTexturesShared 1"));
	});
	
	
	FString Version;
	GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("ProjectVersion"),
		Version,
		GGameIni);
	
	FString Branch;
	GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("BranchName"),
		Branch,
		GGameIni);
	
	std::string content = TCHAR_TO_ANSI(*(Branch + "\n" + Version));
	
	
	auto Ver = IFileManager::Get().CreateFileWriter(*FPaths::Combine(Dir, TEXT("versions")));
	(*Ver).Serialize((uint8*)content.data(), content.length());
	delete Ver;


#ifndef ENABLE_RHI_TRACKING
#define ENABLE_RHI_TRACKING 0
#endif
#if ENABLE_RHI_TRACKING
	GTrackAllocRHITexture = +[](const void* Ptr, uint32 Size, const FString& Name, const FRHITextureCreateDesc& Desc){
		
		int Type = 0;
		switch(Desc.Dimension)
		{
			case ETextureDimension::Texture2D:
				Type = FAllocationTracking::Texture;
				break;
			case ETextureDimension::Texture2DArray:
				Type = FAllocationTracking::TextureArray;
				break;
			case ETextureDimension::Texture3D:
				Type = FAllocationTracking::Texture3D;
				break;
			case ETextureDimension::TextureCube:
				Type = FAllocationTracking::TextureCube;
				break;
			case ETextureDimension::TextureCubeArray:
				Type = FAllocationTracking::TextureCubeArray;
				break;
		}
		
		FAllocationTracking::TrackAllocRHITexture(Ptr, Name, Size, Desc.Extent.X, Desc.Extent.Y, Desc.Depth, Desc.Format, Desc.NumMips, Desc.NumSamples, Desc.ArraySize, Type, bool(Desc.Flags & (ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::DepthStencilTargetable)));
	};
	GTrackAllocRHIBuffer = +[](const void* Ptr, uint32 Size, const FString& Name, const FRHIBufferDesc& Desc){
		int Type = 0;
		if (uint32(Desc.Usage & EBufferUsageFlags::VertexBuffer))
		{
			Type = FAllocationTracking::Vertex;
		}
		else if (uint32(Desc.Usage & EBufferUsageFlags::IndexBuffer))
		{
			Type = FAllocationTracking::Index;
		}
		else
		{
			Type = FAllocationTracking::Struct;
		}
	
		FAllocationTracking::TrackAllocRHIBuffer(Ptr ,Name, Size, Type);
	};
	GTrackFreeRHI = FAllocationTracking::TrackFreeRHIBuffer;

#endif

}
