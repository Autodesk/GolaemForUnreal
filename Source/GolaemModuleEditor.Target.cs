// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class GolaemModuleEditorTarget : TargetRules
{
	public GolaemModuleEditorTarget(TargetInfo Target) : base(Target)
	{
#if UE_5_1_OR_LATER
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
#endif
        DefaultBuildSettings = BuildSettingsVersion.Latest;
		Type = TargetType.Editor;

		ExtraModuleNames.AddRange( new string[] { "GolaemModule" } );
	}
}
