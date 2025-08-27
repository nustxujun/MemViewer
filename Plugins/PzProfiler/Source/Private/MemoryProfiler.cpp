#include "MemoryProfiler.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "AllocationTracking.h"
#include <string>



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
}
