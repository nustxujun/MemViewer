// Copyright 2004-2016 YaS-Online, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MemProfiler : ModuleRules
{
	public MemProfiler( ReadOnlyTargetRules Target ) : base( Target )
	{
		PublicDependencyModuleNames.AddRange( new string[] { "Core",  "CoreUObject", "RenderCore","Engine"} );

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateIncludePaths.Add( Path.Combine( ModuleDirectory, "Private/Platforms/Windows/" ) );
			PrivateIncludePaths.Add( Path.Combine( ModuleDirectory, "Private/Platforms/Windows/MinHook/" ) );
        }
		else if (Target.Platform == UnrealBuildTool.IOS)
		{
			PrivateIncludePaths.Add( Path.Combine( ModuleDirectory, "Private/Platforms/IOS/" ) );
		}
    }
}
