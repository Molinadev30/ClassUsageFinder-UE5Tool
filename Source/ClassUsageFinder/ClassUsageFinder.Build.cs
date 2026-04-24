// Copyright (c) Ruben. Licensed under the MIT License.

using UnrealBuildTool;

public class ClassUsageFinder : ModuleRules
{
	public ClassUsageFinder(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"EditorStyle",
			"EditorFramework",
			"EditorSubsystem",
			"ToolMenus",
			"Projects",
			"AssetRegistry",
			"AssetTools",
			"ContentBrowser",
			"ClassViewer",
			"Kismet",
			"PropertyEditor",
			"WorkspaceMenuStructure",
			"DesktopPlatform",
			"ApplicationCore",
		});
	}
}
