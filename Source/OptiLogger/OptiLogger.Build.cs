// Copyright (c) Victor Rivas Perez. All Rights Reserved.

using UnrealBuildTool;

public class OptiLogger : ModuleRules
{
	public OptiLogger(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The module is declared Type "Editor" in OptiLogger.uplugin, so the editor
		// dependencies below need no bBuildEditor guard - it is never built otherwise.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",

			// Slate UI
			"InputCore",
			"Slate",
			"SlateCore",

			// Editor integration
			"AssetRegistry",
			"EditorSubsystem",
			"LevelEditor",
			"MaterialEditor",
			"ToolMenus",
			"UnrealEd",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
			"Projects",
			"RenderCore",
			"RHI",
		});

		// "UnrealEd" was listed twice in the public list. "EditorStyle" was also listed; it is
		// a deprecated shim in UE5 whose contents moved into SlateCore and EditorWidgets.
	}
}
