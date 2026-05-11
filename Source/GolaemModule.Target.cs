// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class GolaemModuleTarget : TargetRules
{
	public GolaemModuleTarget(TargetInfo Target) : base(Target)
	{
#if UE_5_1_OR_LATER
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
#endif		
	
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		bOverrideBuildEnvironment = true;
		bForceEnableExceptions = true;
		Type = TargetType.Game;

		ExtraModuleNames.AddRange( new string[] { "GolaemModule" } );
	}
}
