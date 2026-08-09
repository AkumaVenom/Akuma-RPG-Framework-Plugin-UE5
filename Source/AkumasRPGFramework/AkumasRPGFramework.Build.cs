using UnrealBuildTool;

public class AkumasRPGFramework : ModuleRules
{
    public AkumasRPGFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "AIModule",
            "NavigationSystem",
            "NetCore",
            "DeveloperSettings",
            "UMG",
            "Slate",
            "SlateCore",
            "Niagara"
        });
    }
}
