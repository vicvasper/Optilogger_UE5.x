// Copyright (c) Victor Rivas Perez. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

class FMenuBuilder;
class FSpawnTabArgs;
class FToolBarBuilder;
class FUICommandList;
class SDockTab;

/**
 * Plugin-wide log category.
 *
 * Analysis passes walk every asset in a level and can emit a lot of detail, so their output
 * needs its own verbosity control ("Log LogOptiLogger Verbose") rather than being mixed into
 * LogTemp with everything else in the editor.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogOptiLogger, Log, All);

/**
 * Editor module entry point: registers the OptiLogger tab, its toolbar button and its
 * Window-menu entry, and tears them down on shutdown.
 */
class FOptiLogger : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	static FOptiLogger& Get()
	{
		return FModuleManager::LoadModuleChecked<FOptiLogger>("OptiLogger");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("OptiLogger");
	}

	/** Brings up the OptiLogger tab, spawning it if it is not already open. */
	void PluginButtonClicked();

private:
	void RegisterMenus();

	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);

	TSharedPtr<FUICommandList> PluginCommands;

	// Removed: AddToolbarExtension and AddMenuExtension, declared here but never defined -
	// menu registration goes through RegisterMenus and the UToolMenus API. Also removed an
	// OptiloggerWidget member that was never assigned or read; the tab owns its widget.
};
