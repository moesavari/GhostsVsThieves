// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GhostsVsThieves : ModuleRules
{
	public GhostsVsThieves(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Run-mode master switch. Set to 1 when development resumes to restore
        // debug input bindings, on-screen diagnostics, and visualization draws.
        PublicDefinitions.Add("GVT_ENABLE_DEBUG_TOOLS=0");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "GameplayTags",
            "UMG",
            "Slate",
            "SlateCore",
            "NetCore",
            "NavigationSystem",
            "AIModule",
            "OnlineSubsystem",
            "OnlineSubsystemUtils",
        });
        PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
