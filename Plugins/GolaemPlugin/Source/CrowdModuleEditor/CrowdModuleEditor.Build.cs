/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

using System.IO;
using UnrealBuildTool;
using Golaem;

public class CrowdModuleEditor : ModuleRules
{
    public CrowdModuleEditor(ReadOnlyTargetRules Target) : base(Target)
    {
		bUseUnity = false;
		PrecompileForTargets = PrecompileTargetsType.Any;
        //bFasterWithoutUnity = true;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/GolaemCrowdEditor.h";

        GolaemMacros.AddGolaemDependencies(this, Target, false, false);

        PrivateIncludePaths.AddRange(
            new string[]{
                "CrowdModule/Private",
                "CrowdModuleEditor/Private",
            }
        );

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
                "CoreUObject",
				"CrowdModule",
				"DesktopPlatform",
				"EditorScriptingUtilities",
				"EditorStyle", //FEditorStyle
                "Engine",  //AActor, USkeletalMeshComponent, UAnimSequenceBase
				"Landscape", //ALandscapeProxy
				"LevelEditor",
				"InputCore", //EKeys::Escape
				"LevelSequence",
				"MainFrame", //IMainFrameModule
				"MeshDescription", //FMeshDescription
				"MeshUtilities",
                "MovieScene", //UMovieScene for FGolaemSection
                "MovieSceneTools", //CommonMovieSceneTools
                "Projects", //IPluginManager
                "PropertyEditor", //IPropertyTypeCustomization
				"RawMesh",
				"RenderCore", //FlushRenderingCommands
				"SceneOutliner", //SceneOutliner
                "Sequencer", //FMovieSceneTrackEditor				
                "Slate", //FUIAction for FGolaemTrackEditor
				"SlateCore", // SlateStyle
				"StaticMeshDescription", //FStaticMeshAttributes				
				"UnrealEd" 
				// ... add private dependencies that you statically link with here ...	
			}
        );

        // SequencerCore for TimeToPixel (UE 5.4+)
        PrivateDependencyModuleNames.Add("SequencerCore");

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
        );
    }
}
