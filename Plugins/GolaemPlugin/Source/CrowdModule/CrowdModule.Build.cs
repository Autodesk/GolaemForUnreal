/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

using System;
using System.IO;
using UnrealBuildTool;
using Golaem;

public class CrowdModule : ModuleRules
{
    public CrowdModule(ReadOnlyTargetRules Target) : base(Target)
    {
		bUseUnity = false;
		PrecompileForTargets = PrecompileTargetsType.Any;
        //bFasterWithoutUnity = true;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/GlmCrowdModule.h";

        // packaging mode?
        string packagingValue = System.Environment.GetEnvironmentVariable("GLM_PACKAGING");
        if (packagingValue == "1") PublicDefinitions.Add("GLM_PACKAGING=1");

        GolaemMacros.AddGolaemDependencies(this, Target, true, true);
        PrivateIncludePaths.Add("CrowdModule/Private");

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                // ... add other public dependencies that you statically link with here ...
			}
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
				"Core",
                "CoreUObject", // GetTransientPackage(), StaticAllocateObject, FFrame, UObject, etc...
				"Engine", // USkeletalMesh
                "Json", //FJsonValue, etc..
				"Landscape", //ALandscapeProxy
				"MeshDescription", //FMeshDescription
				"MovieScene", //UMovieSceneTrack, etc
                "Projects", // IPluginManager
				"RawMesh", // FRawMesh
				"SlateCore", //	FSlateColor, etc.
				"Slate", //STextBlock, SWidget, etc.
				"StaticMeshDescription", //FStaticMeshAttributes
				//"EditorStyle" //FEditorStyle
				// ... add private dependencies that you statically link with here ...	
			}
        );
		
        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
        );		
    }
}