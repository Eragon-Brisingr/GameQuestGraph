using UnrealBuildTool;

public class GameQuestGraphEditor : ModuleRules
{
    public GameQuestGraphEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "AssetTools",
                "GraphEditor",
                "BlueprintGraph",
                "UnrealEd",
                "Kismet",
                "KismetCompiler",
                "ToolMenus",
                "InputCore",
                "PropertyEditor",

                "StructUtils",
                "StructUtilsEditor",
                "GameQuestGraph",
            }
        );
    }
}