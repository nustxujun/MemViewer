#include "Modules/ModuleManager.h"
#include "MemoryProfiler.h"
#include "ObjectProfiler.h"
#include "LuaMemProfiler.h"
#include "AllocationTracking.h"
#include <fstream>

static bool bEnableProfiler = false;

static FString& GetDir()
{
	static FString Dir;
	return Dir;
}

static const TCHAR* CmdLineFile()
{
#if ENGINE_MAJOR_VERSION >= 5
	return TEXT("uecommandline.txt");
#else
	return TEXT("ue4commandline.txt");
#endif
} 


#if PLATFORM_IOS

static auto GetStartCommandLine = [](){
	
	FString Path = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
	Path += CmdLineFile();
	
	FString Cmd;
	auto file = std::fstream(TCHAR_TO_ANSI(*Path),std::ios::in);
	
	auto DateTime = FDateTime::Now().ToString(TEXT("%Y_%m_%d-%H_%M_%S"));
	GetDir() = FPaths::GetPath(Path) + TEXT("/MemTrace/") + DateTime;
	if (file)
	{

		std::string content;
		
		while (!file.eof() && file.good())
		{
			std::string line;
			file >> line ;
			content += line + " ";
		}
		
		
		FString Str = ANSI_TO_TCHAR(content.c_str());
		
		UE_LOG(LogCore, Log, TEXT("Commandline is %s"),*Str);

        if (!FParse::Param(*Str, TEXT("memoryprofile")) )
            return 0;
			
		FMemoryProfiler::Initialize(*Str,*GetDir());
		FUObjectProfiler::Initialize();
		FLuaMemProfiler::Initialize(*Str, *GetDir());
		
		bEnableProfiler = true; 
	}
	

	return 0;
}();
#elif PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
static auto GetStartCommandLine = []() {
	FString Str = ANSI_TO_TCHAR(::GetCommandLineA());
	bEnableProfiler = FParse::Param(*Str, TEXT("memoryprofile")) == true;;

	if (!bEnableProfiler)
		return 0;

	auto Path = FPlatformMisc::ProjectDir();
	GetDir() = FPaths::Combine(Path, TEXT("/MemTrace/")) + FDateTime::Now().ToString(TEXT("%Y_%m_%d-%H_%M_%S"));

	FMemoryProfiler::Initialize(*Str, *GetDir());
	FUObjectProfiler::Initialize();
	FLuaMemProfiler::Initialize(*Str, *GetDir());
	return 0;
}();
#elif PLATFORM_ANDROID
#include <string>
#include <sys/mman.h>
#include <fcntl.h>
static auto GetStartCommandLine = []() {
		//FString Path = FPaths::Combine(FPaths::ProjectPersistentDownloadDir(), FString("UnrealGame") , (!FApp::IsProjectNameEmpty() ? FApp::GetProjectName() : FPlatformProcess::ExecutableName()) );
		//Path = IAndroidPlatformFile::GetPlatformPhysical().FileRootPath(*Path);

		//auto file = std::fstream(TCHAR_TO_UTF8(*FPaths::Combine(Path,CmdLineFile())), std::ios::in);
		//FString Cmd;

		//auto DateTime = FDateTime::Now().ToString(TEXT("%Y_%m_%d-%H_%M_%S"));
		//GetDir() = FPaths::Combine(Path, TEXT("MemTrace"), DateTime);
		//if (file)
		//{

		//	std::string content;

		//	while (!file.eof() && file.good())
		//	{
		//		std::string line;
		//		file >> line;
		//		content += line + " ";
		//	}


		//	FString Str = ANSI_TO_TCHAR(content.c_str());

		//	UE_LOG(LogCore, Log, TEXT("Commandline is %s"), *Str);
		//	if (!FParse::Param(*Str, TEXT("memoryprofile")))
		//		return 0;

		//	FMemoryProfiler::Initialize(*Str, *GetDir());
		//	FUObjectProfiler::Initialize();
		//	FLuaMemProfiler::Initialize(*Str, *GetDir());

		//	bEnableProfiler = true;
		//}
		//else
		//{
		//}

		return 0;
	}();
#endif


class FMemProfilerModule : public IModuleInterface
{
public:
    void StartupModule() override
    {
		
		if (bEnableProfiler)
		{
			UE_LOG(LogCore, Log, TEXT("Profiler is enabled."));
		}
		else
			return;

		
		FMemoryProfiler::Start(*GetDir());
		FUObjectProfiler::Start(*GetDir());
		FLuaMemProfiler::Start(*GetDir());
		
		FCoreDelegates::OnBeginFrame.AddLambda([](){
		   FAllocationTracking::UpdateFrame();
		   FUObjectProfiler::UpdateFrame();
		});

    }
    void ShutdownModule() override
    {
        
    }











};


IMPLEMENT_MODULE( FMemProfilerModule, MemProfiler )
