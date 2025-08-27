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

#if PLATFORM_IOS

static auto GetStartCommandLine = [](){
	bool HasCmdline = false;
	@autoreleasepool {
		HasCmdline = [[NSFileManager defaultManager] fileExistsAtPath:[[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0] stringByAppendingString: @"/ue4commandline.txt"]];
	}
	if (!HasCmdline)
		return 0;
	
	// command line text file pushed to the documents folder
	NSString* NSPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];

	NSPath = [NSPath stringByAppendingString:[NSString stringWithUTF8String:"/ue4commandline.txt"]];
	FString Path = [NSPath UTF8String];
	
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
	auto Dir = FPaths::Combine(Path ,TEXT("/MemTrace/")) + FDateTime::Now().ToString(TEXT("%Y_%m_%d-%H_%M_%S"));

	FMemoryProfiler::Initialize(*Str, *Dir);
	FUObjectProfiler::Initialize();
	FLuaMemProfiler::Initialize(*Str, *Dir);
	return 0;
}();

#endif


class FPzProfilerModule : public IModuleInterface
{
public:
    void StartupModule() override
    {
		
		if (bEnableProfiler)
		{
			UE_LOG(LogCore, Log, TEXT("PzProfiler is enabled."));
		}
		else
			return;

		
		FMemoryProfiler::Start(*GetDir());
		FUObjectProfiler::Start();
		FLuaMemProfiler::Start(*GetDir());
		
		FCoreDelegates::OnBeginFrame.AddLambda([](){
		   FAllocationTracking::BeginFrame();
		});

    }
    void ShutdownModule() override
    {
        
    }











};


IMPLEMENT_MODULE( FPzProfilerModule, PzProfiler )
