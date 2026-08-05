// Copyright (c) Victor Rivas Perez. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "InputCoreTypes.h"

#include "OptiloggerSubsystem.generated.h"

class UResourceAnalyzer;

/**
 * Every action the subsystem can perform.
 *
 * The command set used to exist as bare strings in five places that all had to agree: the
 * key-to-string map, the dispatch chain that compared those strings, the refresh chain, the
 * public string entry point, and the display formatter. A typo in any one of them failed
 * silently. They are one enum and one table now (see the command table in the .cpp).
 */
enum class EOptiloggerCommand : uint8
{
	AnalyzeCurrentLevel,
	AnalyzeStaticMeshes,
	AnalyzeSkeletalMeshes,
	AnalyzeTextures,
	AnalyzeMaterials,
	AnalyzeAnimations,
	AnalyzeAudio,
	AnalyzeLighting,
	AnalyzePostProcess,
	ExportReport,
	ClearResults,
	RefreshAnalysis,
	ToggleDisplay,

	None
};

/**
 * Editor subsystem owning the resource analyzer, its hotkeys and the on-screen summary.
 */
UCLASS()
class UOptiloggerSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** Runs an analysis by its stable identifier ("StaticMeshes", "CurrentLevel", ...). */
	void TriggerAnalysis(const FString& AnalysisType);

	/** Runs any command directly. Preferred over the string form from C++. */
	void ExecuteCommand(EOptiloggerCommand Command);

	bool ExportCurrentAnalysis();

	void DisplayAnalysisResults();
	void HideAnalysisResults();
	void ToggleAnalysisDisplay();

	/** Human-readable summary of the most recent analysis. */
	FString GetAnalysisSummary() const;

	UResourceAnalyzer* GetResourceAnalyzer() const { return ResourceAnalyzer; }

	/** Identifier of the last analysis run, or empty if none. */
	FString GetLastAnalysisType() const;

	FDateTime GetLastAnalysisTime() const { return LastAnalysisTime; }

	/** Forgets the last analysis, so the display and refresh have nothing to act on. */
	void ClearAnalysisState();

	bool IsFilterByVisible() const { return bFilterByVisible; }

	/** Sets the visible-only filter and re-runs the last analysis under it, if there was one. */
	void SetFilterByVisible(bool bVisible);

private:
	void SetupInputHandling();
	void CleanupInputHandling();
	void HandleKeyPress(const FKeyEvent& KeyEvent);

	/**
	 * Shared body of every analysis command: guards, timestamps, runs the analyzer pass the
	 * command names, and refreshes the display. Replaces ten near-identical methods.
	 */
	void RunAnalysis(EOptiloggerCommand Command);

	void ShowOnScreenMessage(const FString& Message, float Duration) const;
	void UpdateAnalysisDisplay();
	FString FormatAnalysisData() const;

	/** Renders the hotkey list from the command table, so it cannot drift from the bindings. */
	FString FormatControlsHelp() const;

	UPROPERTY()
	TObjectPtr<UResourceAnalyzer> ResourceAnalyzer;

	/** The last analysis run, driving both the display and RefreshAnalysis. */
	EOptiloggerCommand LastAnalysisCommand = EOptiloggerCommand::None;
	FDateTime LastAnalysisTime;

	FString CurrentDisplayText;
	FDelegateHandle KeyDownDelegateHandle;

	bool bInputHandlingEnabled = false;
	bool bAnalysisDisplayVisible = false;
	bool bFilterByVisible = false;
};
