// Copyright 2004-2016 YaS-Online, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MemProfiler : ModuleRules
{
	public MemProfiler( ReadOnlyTargetRules Target ) : base( Target )
	{
		PublicDependencyModuleNames.AddRange( new string[] { "Core",  "CoreUObject", "RenderCore","Engine"} );

		PublicDependencyModuleNames.Add("RHI");
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "MemProfiler_UPL.xml"));
		}
    }
}
