// Copyright (c) Victor Rivas Perez. All Rights Reserved.

#include "OptiloggerSubsystem.h"

#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/DateTime.h"
#include "OptiLogger.h"
#include "ResourceAnalyzer.h"

namespace
{
	/** Seconds an on-screen message stays up. */
	constexpr float TransientMessageDuration = 2.0f;
	constexpr float ResultMessageDuration = 3.0f;

	/** Passed to AddOnScreenDebugMessage as the key: -1 appends rather than replacing. */
	constexpr uint64 AppendOnScreenMessage = static_cast<uint64>(-1);

	/**
	 * The single definition of what OptiLogger can do.
	 *
	 * Every field that used to be duplicated across the key map, the two dispatch chains and
	 * the hard-coded help text lives here, so a new command is one row rather than six edits
	 * that must stay in agreement.
	 *
	 * AnalyzeFunction is null for commands that are not analysis passes.
	 */
	struct FOptiloggerCommandDef
	{
		EOptiloggerCommand Command;

		/** Stable identifier, used by the string-based public API and by saved state. */
		const TCHAR* Id;

		FKey Key;

		/** Whether the binding requires Ctrl. Part of the binding, not a special case. */
		bool bRequiresCtrl;

		/** How the binding reads in the help text. */
		const TCHAR* KeyLabel;

		/** Noun phrase used in progress messages: "Analyzing <label>...". */
		const TCHAR* Label;

		void (UResourceAnalyzer::*AnalyzeFunction)(bool);
	};

	/**
	 * Built inside a function so it is constructed on first use. EKeys' globals are themselves
	 * statics; a namespace-scope table would depend on their initialisation order.
	 */
	const TArray<FOptiloggerCommandDef>& GetCommandTable()
	{
		static const TArray<FOptiloggerCommandDef> Table =
		{
			{ EOptiloggerCommand::AnalyzeCurrentLevel,   TEXT("CurrentLevel"),   EKeys::NumPadOne,   true, TEXT("Ctrl+NumPad1"), TEXT("current level"),        &UResourceAnalyzer::AnalyzeCurrentLevel        },
			{ EOptiloggerCommand::ExportReport,          TEXT("ExportReport"),   EKeys::NumPadTwo,   true, TEXT("Ctrl+NumPad2"), TEXT("report"),               nullptr                                        },
			{ EOptiloggerCommand::AnalyzeStaticMeshes,   TEXT("StaticMeshes"),   EKeys::NumPadThree, true, TEXT("Ctrl+NumPad3"), TEXT("static meshes"),        &UResourceAnalyzer::AnalyzeStaticMeshes        },
			{ EOptiloggerCommand::AnalyzeSkeletalMeshes, TEXT("SkeletalMeshes"), EKeys::NumPadFour,  true, TEXT("Ctrl+NumPad4"), TEXT("skeletal meshes"),      &UResourceAnalyzer::AnalyzeSkeletalMeshes      },
			{ EOptiloggerCommand::AnalyzeTextures,       TEXT("Textures"),       EKeys::NumPadFive,  true, TEXT("Ctrl+NumPad5"), TEXT("textures"),             &UResourceAnalyzer::AnalyzeTextures            },
			{ EOptiloggerCommand::AnalyzeMaterials,      TEXT("Materials"),      EKeys::NumPadSix,   true, TEXT("Ctrl+NumPad6"), TEXT("materials"),            &UResourceAnalyzer::AnalyzeMaterials           },
			{ EOptiloggerCommand::AnalyzeAnimations,     TEXT("Animations"),     EKeys::NumPadSeven, true, TEXT("Ctrl+NumPad7"), TEXT("animations"),           &UResourceAnalyzer::AnalyzeAnimations          },
			{ EOptiloggerCommand::AnalyzeAudio,          TEXT("Audio"),          EKeys::NumPadEight, true, TEXT("Ctrl+NumPad8"), TEXT("audio"),                &UResourceAnalyzer::AnalyzeAudio               },
			{ EOptiloggerCommand::AnalyzeLighting,       TEXT("Lighting"),       EKeys::NumPadNine,  true, TEXT("Ctrl+NumPad9"), TEXT("lighting"),             &UResourceAnalyzer::AnalyzeLighting            },
			{ EOptiloggerCommand::AnalyzePostProcess,    TEXT("PostProcess"),    EKeys::NumPadZero,  true, TEXT("Ctrl+NumPad0"), TEXT("post-process effects"), &UResourceAnalyzer::AnalyzePostProcessEffects  },
			{ EOptiloggerCommand::ClearResults,          TEXT("ClearResults"),   EKeys::Delete,      true, TEXT("Ctrl+Delete"),  TEXT("results"),              nullptr                                        },

			// Ctrl is required here too. These were bound bare, and because the handler runs on
			// Slate's pre-input listener that made F5 and F6 trigger a full analysis from
			// anywhere in the editor - including while typing into a text field.
			{ EOptiloggerCommand::RefreshAnalysis,       TEXT("RefreshAnalysis"), EKeys::F5,         true, TEXT("Ctrl+F5"),      TEXT("analysis"),             nullptr                                        },
			{ EOptiloggerCommand::ToggleDisplay,         TEXT("ToggleDisplay"),   EKeys::F6,         true, TEXT("Ctrl+F6"),      TEXT("display"),              nullptr                                        },
		};

		return Table;
	}

	const FOptiloggerCommandDef* FindCommand(EOptiloggerCommand Command)
	{
		return GetCommandTable().FindByPredicate(
			[Command](const FOptiloggerCommandDef& Def) { return Def.Command == Command; });
	}

	const FOptiloggerCommandDef* FindCommandById(const FString& Id)
	{
		return GetCommandTable().FindByPredicate(
			[&Id](const FOptiloggerCommandDef& Def) { return Id.Equals(Def.Id); });
	}

	const FOptiloggerCommandDef* FindCommandByKey(const FKey& Key, bool bCtrlDown)
	{
		return GetCommandTable().FindByPredicate(
			[&Key, bCtrlDown](const FOptiloggerCommandDef& Def)
			{
				return Def.Key == Key && Def.bRequiresCtrl == bCtrlDown;
			});
	}
}

// ---------------------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------------------

void UOptiloggerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ResourceAnalyzer = NewObject<UResourceAnalyzer>(this);
	ResourceAnalyzer->Initialize();

	SetupInputHandling();

	UE_LOG(LogOptiLogger, Log, TEXT("Subsystem initialised with %d commands."), GetCommandTable().Num());
}

void UOptiloggerSubsystem::Deinitialize()
{
	CleanupInputHandling();

	if (ResourceAnalyzer)
	{
		ResourceAnalyzer->ClearAnalysisResults();
	}

	// The base implementation was never called, so USubsystem's own teardown never ran.
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------

void UOptiloggerSubsystem::SetupInputHandling()
{
	if (bInputHandlingEnabled || !FSlateApplication::IsInitialized())
	{
		return;
	}

	KeyDownDelegateHandle = FSlateApplication::Get()
		.OnApplicationPreInputKeyDownListener()
		.AddUObject(this, &UOptiloggerSubsystem::HandleKeyPress);

	bInputHandlingEnabled = true;
}

void UOptiloggerSubsystem::CleanupInputHandling()
{
	if (!bInputHandlingEnabled)
	{
		return;
	}

	if (FSlateApplication::IsInitialized() && KeyDownDelegateHandle.IsValid())
	{
		FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(KeyDownDelegateHandle);
	}

	KeyDownDelegateHandle.Reset();
	bInputHandlingEnabled = false;
}

void UOptiloggerSubsystem::HandleKeyPress(const FKeyEvent& KeyEvent)
{
	if (KeyEvent.IsRepeat())
	{
		return;
	}

	// Ctrl is matched as part of the binding rather than tested separately, so a command's
	// modifier requirement lives with the command.
	const bool bCtrlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();

	if (const FOptiloggerCommandDef* Def = FindCommandByKey(KeyEvent.GetKey(), bCtrlDown))
	{
		ExecuteCommand(Def->Command);
	}
}

// ---------------------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------------------

void UOptiloggerSubsystem::ExecuteCommand(EOptiloggerCommand Command)
{
	switch (Command)
	{
	case EOptiloggerCommand::ExportReport:
	{
		if (!ResourceAnalyzer)
		{
			ShowOnScreenMessage(TEXT("Resource analyzer is not available"), ResultMessageDuration);
			return;
		}

		ShowOnScreenMessage(
			ResourceAnalyzer->ExportAnalysisToJSON()
				? TEXT("Analysis report exported")
				: TEXT("Failed to export the analysis report"),
			ResultMessageDuration);
		return;
	}

	case EOptiloggerCommand::ClearResults:
	{
		if (!ResourceAnalyzer)
		{
			return;
		}

		ResourceAnalyzer->ClearAnalysisResults();
		ClearAnalysisState();
		ShowOnScreenMessage(TEXT("Analysis results cleared"), TransientMessageDuration);
		return;
	}

	case EOptiloggerCommand::RefreshAnalysis:
	{
		if (LastAnalysisCommand == EOptiloggerCommand::None)
		{
			ShowOnScreenMessage(TEXT("No previous analysis to refresh"), TransientMessageDuration);
			return;
		}

		RunAnalysis(LastAnalysisCommand);
		return;
	}

	case EOptiloggerCommand::ToggleDisplay:
		ToggleAnalysisDisplay();
		return;

	default:
		RunAnalysis(Command);
		return;
	}
}

void UOptiloggerSubsystem::RunAnalysis(EOptiloggerCommand Command)
{
	const FOptiloggerCommandDef* Def = FindCommand(Command);
	if (!Def || !Def->AnalyzeFunction || !ResourceAnalyzer)
	{
		return;
	}

	LastAnalysisCommand = Command;
	LastAnalysisTime = FDateTime::Now();

	// The analyzer pass runs synchronously on the game thread, so no frame is presented while
	// it works. A "starting" message would never reach the screen; only the result is shown.
	(ResourceAnalyzer->*(Def->AnalyzeFunction))(bFilterByVisible);

	ShowOnScreenMessage(
		FString::Printf(TEXT("Analyzed %s"), Def->Label), ResultMessageDuration);

	if (bAnalysisDisplayVisible)
	{
		UpdateAnalysisDisplay();
	}
}

void UOptiloggerSubsystem::TriggerAnalysis(const FString& AnalysisType)
{
	const FOptiloggerCommandDef* Def = FindCommandById(AnalysisType);
	if (!Def)
	{
		UE_LOG(LogOptiLogger, Warning, TEXT("Unknown analysis type '%s'."), *AnalysisType);
		return;
	}

	ExecuteCommand(Def->Command);
}

// ---------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------

FString UOptiloggerSubsystem::GetLastAnalysisType() const
{
	const FOptiloggerCommandDef* Def = FindCommand(LastAnalysisCommand);
	return Def ? FString(Def->Id) : FString();
}

void UOptiloggerSubsystem::ClearAnalysisState()
{
	LastAnalysisCommand = EOptiloggerCommand::None;
	CurrentDisplayText.Empty();

	if (bAnalysisDisplayVisible)
	{
		UpdateAnalysisDisplay();
	}
}

void UOptiloggerSubsystem::SetFilterByVisible(bool bVisible)
{
	if (bFilterByVisible == bVisible)
	{
		return;
	}

	bFilterByVisible = bVisible;

	// Re-run under the new filter so the displayed results match the setting.
	if (LastAnalysisCommand != EOptiloggerCommand::None)
	{
		RunAnalysis(LastAnalysisCommand);
	}
}

bool UOptiloggerSubsystem::ExportCurrentAnalysis()
{
	return ResourceAnalyzer && ResourceAnalyzer->ExportAnalysisToJSON();
}

// ---------------------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------------------

void UOptiloggerSubsystem::DisplayAnalysisResults()
{
	bAnalysisDisplayVisible = true;
	UpdateAnalysisDisplay();
}

void UOptiloggerSubsystem::HideAnalysisResults()
{
	bAnalysisDisplayVisible = false;
	CurrentDisplayText.Empty();
}

void UOptiloggerSubsystem::ToggleAnalysisDisplay()
{
	if (bAnalysisDisplayVisible)
	{
		HideAnalysisResults();
		ShowOnScreenMessage(TEXT("Analysis display hidden"), TransientMessageDuration);
	}
	else
	{
		DisplayAnalysisResults();
		ShowOnScreenMessage(TEXT("Analysis display shown"), TransientMessageDuration);
	}
}

void UOptiloggerSubsystem::UpdateAnalysisDisplay()
{
	if (!bAnalysisDisplayVisible)
	{
		return;
	}

	CurrentDisplayText = FormatAnalysisData();
}

FString UOptiloggerSubsystem::GetAnalysisSummary() const
{
	return ResourceAnalyzer ? FormatAnalysisData() : TEXT("Resource analyzer is not available");
}

FString UOptiloggerSubsystem::FormatAnalysisData() const
{
	if (!ResourceAnalyzer)
	{
		return TEXT("No analysis data available");
	}

	FString DisplayText = TEXT("=== OPTILOGGER ANALYSIS RESULTS ===\n\n");

	if (LastAnalysisCommand != EOptiloggerCommand::None)
	{
		DisplayText += FString::Printf(TEXT("Last Analysis: %s\n"), *GetLastAnalysisType());
		DisplayText += FString::Printf(TEXT("Analysis Time: %s\n"), *LastAnalysisTime.ToString());
		DisplayText += FString::Printf(TEXT("Filter: %s\n\n"),
			bFilterByVisible ? TEXT("Visible only") : TEXT("All in level"));
	}

	float TotalMemoryMB = 0.0f;

	// Sums the estimate over any result set exposing EstimatedMemoryUsageMB, so the running
	// total no longer has to be accumulated by hand in each branch - which is how three of
	// them ended up reporting 0.00 MB despite listing assets with a per-asset estimate.
	const auto AccumulateMemory = [&TotalMemoryMB](const auto& Results)
	{
		for (const auto& Data : Results)
		{
			TotalMemoryMB += Data.EstimatedMemoryUsageMB;
		}
	};

	switch (LastAnalysisCommand)
	{
	case EOptiloggerCommand::AnalyzeCurrentLevel:
		DisplayText += FString::Printf(TEXT("Static Meshes: %d\n"), ResourceAnalyzer->GetStaticMeshAnalysisResults().Num());
		DisplayText += FString::Printf(TEXT("Skeletal Meshes: %d\n"), ResourceAnalyzer->GetSkeletalMeshAnalysisResults().Num());
		DisplayText += FString::Printf(TEXT("Textures: %d\n"), ResourceAnalyzer->GetTextureAnalysisResults().Num());
		DisplayText += FString::Printf(TEXT("Materials: %d\n"), ResourceAnalyzer->GetMaterialAnalysisResults().Num());
		DisplayText += FString::Printf(TEXT("Animations: %d\n"), ResourceAnalyzer->GetAnimationAnalysisResults().Num());
		DisplayText += FString::Printf(TEXT("Audio Assets: %d\n"), ResourceAnalyzer->GetAudioAnalysisResults().Num());
		DisplayText += FString::Printf(TEXT("Lights: %d\n"), ResourceAnalyzer->GetLightingAnalysisResults().Num());
		DisplayText += FString::Printf(TEXT("Post-Process Volumes: %d\n\n"), ResourceAnalyzer->GetPostProcessAnalysisResults().Num());

		// Animations and audio carry an estimate too, and were previously left out of the
		// level total.
		AccumulateMemory(ResourceAnalyzer->GetStaticMeshAnalysisResults());
		AccumulateMemory(ResourceAnalyzer->GetSkeletalMeshAnalysisResults());
		AccumulateMemory(ResourceAnalyzer->GetTextureAnalysisResults());
		AccumulateMemory(ResourceAnalyzer->GetAnimationAnalysisResults());
		AccumulateMemory(ResourceAnalyzer->GetAudioAnalysisResults());
		break;

	case EOptiloggerCommand::AnalyzeStaticMeshes:
		DisplayText += FString::Printf(TEXT("Static Meshes: %d\n\n"), ResourceAnalyzer->GetStaticMeshAnalysisResults().Num());
		for (const FStaticMeshAnalysisData& Data : ResourceAnalyzer->GetStaticMeshAnalysisResults())
		{
			DisplayText += FString::Printf(TEXT("- %s: Verts %d, Tris %d, LODs %d, Mem %.2f MB\n"),
				*Data.AssetName, Data.VertexCount, Data.TriangleCount, Data.LODCount, Data.EstimatedMemoryUsageMB);
		}
		AccumulateMemory(ResourceAnalyzer->GetStaticMeshAnalysisResults());
		break;

	case EOptiloggerCommand::AnalyzeSkeletalMeshes:
		DisplayText += FString::Printf(TEXT("Skeletal Meshes: %d\n\n"), ResourceAnalyzer->GetSkeletalMeshAnalysisResults().Num());
		for (const FSkeletalMeshAnalysisData& Data : ResourceAnalyzer->GetSkeletalMeshAnalysisResults())
		{
			DisplayText += FString::Printf(TEXT("- %s: Bones %d, Verts %d, LODs %d, Mem %.2f MB\n"),
				*Data.AssetName, Data.BoneCount, Data.VertexCount, Data.LODCount, Data.EstimatedMemoryUsageMB);
		}
		AccumulateMemory(ResourceAnalyzer->GetSkeletalMeshAnalysisResults());
		break;

	case EOptiloggerCommand::AnalyzeTextures:
		DisplayText += FString::Printf(TEXT("Textures: %d\n\n"), ResourceAnalyzer->GetTextureAnalysisResults().Num());
		for (const FTextureAnalysisData& Data : ResourceAnalyzer->GetTextureAnalysisResults())
		{
			DisplayText += FString::Printf(TEXT("- %s: %dx%d, %s, Mem %.2f MB\n"),
				*Data.AssetName, Data.Width, Data.Height, *Data.CompressionFormat, Data.EstimatedMemoryUsageMB);
		}
		AccumulateMemory(ResourceAnalyzer->GetTextureAnalysisResults());
		break;

	case EOptiloggerCommand::AnalyzeMaterials:
		DisplayText += FString::Printf(TEXT("Materials: %d\n\n"), ResourceAnalyzer->GetMaterialAnalysisResults().Num());
		for (const FMaterialAnalysisData& Data : ResourceAnalyzer->GetMaterialAnalysisResults())
		{
			DisplayText += FString::Printf(TEXT("- %s: Textures %d, Total Inst %d, %s\n"),
				*Data.AssetName, Data.TextureReferences, Data.TotalShaderInstructions, *Data.ComplexityLevel);
		}
		break;

	case EOptiloggerCommand::AnalyzeAnimations:
		DisplayText += FString::Printf(TEXT("Animations: %d\n\n"), ResourceAnalyzer->GetAnimationAnalysisResults().Num());
		for (const FAnimationAnalysisData& Data : ResourceAnalyzer->GetAnimationAnalysisResults())
		{
			DisplayText += FString::Printf(TEXT("- %s: Duration %.1fs, Keys %d, Mem %.2f MB\n"),
				*Data.AssetName, Data.Duration, Data.KeyframeCount, Data.EstimatedMemoryUsageMB);
		}
		AccumulateMemory(ResourceAnalyzer->GetAnimationAnalysisResults());
		break;

	case EOptiloggerCommand::AnalyzeAudio:
		DisplayText += FString::Printf(TEXT("Audio Assets: %d\n\n"), ResourceAnalyzer->GetAudioAnalysisResults().Num());
		for (const FAudioAnalysisData& Data : ResourceAnalyzer->GetAudioAnalysisResults())
		{
			DisplayText += FString::Printf(TEXT("- %s: Duration %.1fs, Sample %d Hz, Mem %.2f MB\n"),
				*Data.AssetName, Data.Duration, Data.SampleRate, Data.EstimatedMemoryUsageMB);
		}
		AccumulateMemory(ResourceAnalyzer->GetAudioAnalysisResults());
		break;

	case EOptiloggerCommand::AnalyzeLighting:
		DisplayText += FString::Printf(TEXT("Lights: %d\n\n"), ResourceAnalyzer->GetLightingAnalysisResults().Num());
		for (const FLightingAnalysisData& Data : ResourceAnalyzer->GetLightingAnalysisResults())
		{
			DisplayText += FString::Printf(TEXT("- %s: Type %s, Mobility %s\n"),
				*Data.LightName, *Data.LightType, *Data.Mobility);
		}
		break;

	case EOptiloggerCommand::AnalyzePostProcess:
		DisplayText += FString::Printf(TEXT("Post-Process Volumes: %d\n\n"), ResourceAnalyzer->GetPostProcessAnalysisResults().Num());
		for (const FPostProcessAnalysisData& Data : ResourceAnalyzer->GetPostProcessAnalysisResults())
		{
			DisplayText += FString::Printf(TEXT("- %s: Priority %.1f, Effects %d\n"),
				*Data.VolumeName, Data.Priority, Data.ActiveEffects.Num());
		}
		break;

	default:
		DisplayText += TEXT("No analysis has been run yet.\n");
		break;
	}

	DisplayText += FString::Printf(TEXT("\nEstimated Total Memory: %.2f MB\n\n"), TotalMemoryMB);
	DisplayText += FormatControlsHelp();

	return DisplayText;
}

FString UOptiloggerSubsystem::FormatControlsHelp() const
{
	// Generated from the same table that installs the bindings. The list used to be thirteen
	// hard-coded lines, which is a second place to edit and no way to notice when it drifts.
	FString Help = TEXT("=== CONTROLS ===\n");

	for (const FOptiloggerCommandDef& Def : GetCommandTable())
	{
		Help += FString::Printf(TEXT("%s: %s\n"), Def.KeyLabel, Def.Id);
	}

	return Help;
}

void UOptiloggerSubsystem::ShowOnScreenMessage(const FString& Message, float Duration) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(AppendOnScreenMessage, Duration, FColor::Green,
			FString::Printf(TEXT("OptiLogger: %s"), *Message));
	}
}
