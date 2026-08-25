// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FinalProject : ModuleRules
{
	public FinalProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 模块根目录加入 include path，使子目录类可互相 #include "子目录/头文件.h"
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NetCore", "InputCore", "EnhancedInput", "GameplayAbilities", "GameplayTasks", "GameplayTags", "UMG", "AdvancedWidgets", "Slate", "SlateCore", "AIModule", "NavigationSystem", "OnlineSubsystem", "OnlineSubsystemUtils" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
