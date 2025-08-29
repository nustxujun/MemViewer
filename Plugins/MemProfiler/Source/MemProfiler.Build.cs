// Copyright 2004-2016 YaS-Online, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MemProfiler : ModuleRules
{
	public MemProfiler( ReadOnlyTargetRules Target ) : base( Target )
	{
		PublicDependencyModuleNames.AddRange( new string[] { "Core",  "CoreUObject", "RenderCore","Engine"} );
		PrivateDependencyModuleNames.AddRange( new string[] 
		{ 
		} );
	}
}
