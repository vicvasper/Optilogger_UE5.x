// OptiloggerSubsystem.cpp
#include "OptiloggerSubsystem.h"
#include "ResourceAnalyzer.h"
#include "OptiLogger.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "Engine/GameViewportClient.h"
#include "Misc/DateTime.h"

class FSlateApplication;

void UOptiloggerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ResourceAnalyzer = NewObject<UResourceAnalyzer>(this);
    if (ResourceAnalyzer) ResourceAnalyzer->Initialize();
    SetupInputHandling();
    UE_LOG(LogOptiLogger, Log, TEXT("Optilogger: Initialized"));
}

void UOptiloggerSubsystem::Deinitialize()
{
    CleanupInputHandling();
    
    if (ResourceAnalyzer)
    {
        ResourceAnalyzer->ClearAnalysisResults();
    }
    
    UE_LOG(LogOptiLogger, Log, TEXT("Optilogger Subsystem: Deinitialize completed"));
}

void UOptiloggerSubsystem::SetupInputHandling()
{
    if (bInputHandlingEnabled)
    {
        return;
    }

    // Definir mapeo de teclas
    KeyMappings.Empty();
    KeyMappings.Add(EKeys::NumPadOne,   TEXT("AnalyzeCurrentLevel"));
    KeyMappings.Add(EKeys::NumPadTwo,   TEXT("ExportAnalysisReport"));
    KeyMappings.Add(EKeys::NumPadThree, TEXT("AnalyzeStaticMeshes"));
    KeyMappings.Add(EKeys::NumPadFour,  TEXT("AnalyzeSkeletalMeshes"));
    KeyMappings.Add(EKeys::NumPadFive,  TEXT("AnalyzeTextures"));
    KeyMappings.Add(EKeys::NumPadSix,   TEXT("AnalyzeMaterials"));
    KeyMappings.Add(EKeys::NumPadSeven, TEXT("AnalyzeAnimations"));
    KeyMappings.Add(EKeys::NumPadEight, TEXT("AnalyzeAudio"));
    KeyMappings.Add(EKeys::NumPadNine,  TEXT("AnalyzeLighting"));
    KeyMappings.Add(EKeys::NumPadZero,  TEXT("AnalyzePostProcess"));
    KeyMappings.Add(EKeys::Delete,      TEXT("ClearResults"));
    KeyMappings.Add(EKeys::F5,          TEXT("RefreshAnalysis"));
    KeyMappings.Add(EKeys::F6,          TEXT("ToggleDisplay"));

    // Registrar listeners en Slate
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication& SlateApp = FSlateApplication::Get();

        KeyDownDelegateHandle = SlateApp
            .OnApplicationPreInputKeyDownListener()
            .AddUObject(this, &UOptiloggerSubsystem::HandleKeyPress);
        
    }

    bInputHandlingEnabled = true;
    UE_LOG(LogOptiLogger, Log, TEXT("Optilogger Subsystem: Input handling setup completed"));
}

void UOptiloggerSubsystem::CleanupInputHandling()
{
    if (!bInputHandlingEnabled)
    {
        return;
    }
    
    // Unregister input event handlers
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication& SlateApp = FSlateApplication::Get();
        
        if (KeyDownDelegateHandle.IsValid())
        {
            SlateApp.OnApplicationPreInputKeyDownListener().Remove(KeyDownDelegateHandle);
            KeyDownDelegateHandle.Reset();
        }
    }
    
    KeyMappings.Empty();
    bInputHandlingEnabled = false;
    
    UE_LOG(LogOptiLogger, Log, TEXT("Optilogger Subsystem: Input handling cleanup completed"));
}

void UOptiloggerSubsystem::HandleKeyPress(const FKeyEvent& KeyEvent)
{
    // Don't process repeated key presses
    if (KeyEvent.IsRepeat())
    {
        return;
    }
    
    FKey Key = KeyEvent.GetKey();
    
    // Check if Ctrl is pressed for most commands (except F5 and F6)
    bool bCtrlPressed = FSlateApplication::Get().GetModifierKeys().IsControlDown();
    bool bRequiresCtrl = (Key != EKeys::F5 && Key != EKeys::F6);
    
    if (bRequiresCtrl && !bCtrlPressed)
    {
        return;
    }
    
    // Check if this is a mapped key
    if (KeyMappings.Contains(Key))
    {
        ProcessInputCommand(Key);
    }
}

void UOptiloggerSubsystem::HandleKeyRelease(const FKeyEvent& KeyEvent)
{
    // Currently not handling key release events
}

void UOptiloggerSubsystem::ProcessInputCommand(const FKey& Key)
{
    if (!KeyMappings.Contains(Key))
    {
        return;
    }
    
    FString Command = KeyMappings[Key];
    
    UE_LOG(LogOptiLogger, Log, TEXT("Optilogger Subsystem: Processing input command: %s"), *Command);
    
    // Execute the appropriate command
    if (Command == TEXT("AnalyzeCurrentLevel"))
    {
        OnAnalyzeCurrentLevel();
    }
    else if (Command == TEXT("ExportAnalysisReport"))
    {
        OnExportAnalysisReport();
    }
    else if (Command == TEXT("AnalyzeStaticMeshes"))
    {
        OnAnalyzeStaticMeshes();
    }
    else if (Command == TEXT("AnalyzeSkeletalMeshes"))
    {
        OnAnalyzeSkeletalMeshes();
    }
    else if (Command == TEXT("AnalyzeTextures"))
    {
        OnAnalyzeTextures();
    }
    else if (Command == TEXT("AnalyzeMaterials"))
    {
        OnAnalyzeMaterials();
    }
    else if (Command == TEXT("AnalyzeAnimations"))
    {
        OnAnalyzeAnimations();
    }
    else if (Command == TEXT("AnalyzeAudio"))
    {
        OnAnalyzeAudio();
    }
    else if (Command == TEXT("AnalyzeLighting"))
    {
        OnAnalyzeLighting();
    }
    else if (Command == TEXT("AnalyzePostProcess"))
    {
        OnAnalyzePostProcess();
    }
    else if (Command == TEXT("ClearResults"))
    {
        OnClearResults();
    }
    else if (Command == TEXT("RefreshAnalysis"))
    {
        OnRefreshAnalysis();
    }
    else if (Command == TEXT("ToggleDisplay"))
    {
        OnToggleDisplay();
    }
}

void UOptiloggerSubsystem::OnAnalyzeCurrentLevel()
{
    if (!ResourceAnalyzer || bAnalysisInProgress)
    {
        return;
    }
    
    bAnalysisInProgress = true;
    LastAnalysisType = TEXT("CurrentLevel");  // Changed to no space
    LastAnalysisTime = FDateTime::Now();
    
    ShowOnScreenMessage(TEXT("Analyzing current level..."), 2.0f);
    
    ResourceAnalyzer->AnalyzeCurrentLevel(bFilterByVisible);
    
    bAnalysisInProgress = false;
    ShowOnScreenMessage(TEXT("Level analysis completed!"), 3.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnExportAnalysisReport()
{
    if (!ResourceAnalyzer)
    {
        ShowOnScreenMessage(TEXT("Resource analyzer not available"), 3.0f);
        return;
    }
    
    ShowOnScreenMessage(TEXT("Exporting analysis report..."), 2.0f);
    
    bool bSuccess = ResourceAnalyzer->ExportAnalysisToJSON();
    
    if (bSuccess)
    {
        ShowOnScreenMessage(TEXT("Analysis report exported successfully!"), 3.0f);
    }
    else
    {
        ShowOnScreenMessage(TEXT("Failed to export analysis report"), 3.0f);
    }
}

void UOptiloggerSubsystem::OnAnalyzeStaticMeshes()
{
    if (!ResourceAnalyzer || bAnalysisInProgress)
    {
        return;
    }
    
    bAnalysisInProgress = true;
    LastAnalysisType = TEXT("StaticMeshes");  // Changed to no space
    LastAnalysisTime = FDateTime::Now();
    
    ShowOnScreenMessage(TEXT("Analyzing static meshes..."), 2.0f);
    
    ResourceAnalyzer->AnalyzeStaticMeshes(bFilterByVisible);
    
    bAnalysisInProgress = false;
    ShowOnScreenMessage(TEXT("Static mesh analysis completed!"), 3.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnAnalyzeSkeletalMeshes()
{
    if (!ResourceAnalyzer || bAnalysisInProgress)
    {
        return;
    }
    
    bAnalysisInProgress = true;
    LastAnalysisType = TEXT("SkeletalMeshes");  // Changed to no space
    LastAnalysisTime = FDateTime::Now();
    
    ShowOnScreenMessage(TEXT("Analyzing skeletal meshes..."), 2.0f);
    
    ResourceAnalyzer->AnalyzeSkeletalMeshes(bFilterByVisible);
    
    bAnalysisInProgress = false;
    ShowOnScreenMessage(TEXT("Skeletal mesh analysis completed!"), 3.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnAnalyzeTextures()
{
    if (!ResourceAnalyzer || bAnalysisInProgress)
    {
        return;
    }
    
    bAnalysisInProgress = true;
    LastAnalysisType = TEXT("Textures");
    LastAnalysisTime = FDateTime::Now();
    
    ShowOnScreenMessage(TEXT("Analyzing textures..."), 2.0f);
    
    ResourceAnalyzer->AnalyzeTextures(bFilterByVisible);
    
    bAnalysisInProgress = false;
    ShowOnScreenMessage(TEXT("Texture analysis completed!"), 3.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnAnalyzeMaterials()
{
    if (!ResourceAnalyzer || bAnalysisInProgress)
    {
        return;
    }
    
    bAnalysisInProgress = true;
    LastAnalysisType = TEXT("Materials");
    LastAnalysisTime = FDateTime::Now();
    
    ShowOnScreenMessage(TEXT("Analyzing materials..."), 2.0f);
    
    ResourceAnalyzer->AnalyzeMaterials(bFilterByVisible);
    
    bAnalysisInProgress = false;
    ShowOnScreenMessage(TEXT("Material analysis completed!"), 3.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnAnalyzeAnimations()
{
    if (!ResourceAnalyzer || bAnalysisInProgress)
    {
        return;
    }
    
    bAnalysisInProgress = true;
    LastAnalysisType = TEXT("Animations");
    LastAnalysisTime = FDateTime::Now();
    
    ShowOnScreenMessage(TEXT("Analyzing animations..."), 2.0f);
    
    ResourceAnalyzer->AnalyzeAnimations(bFilterByVisible);
    
    bAnalysisInProgress = false;
    ShowOnScreenMessage(TEXT("Animation analysis completed!"), 3.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnAnalyzeAudio()
{
    if (!ResourceAnalyzer || bAnalysisInProgress)
    {
        return;
    }
    
    bAnalysisInProgress = true;
    LastAnalysisType = TEXT("Audio");
    LastAnalysisTime = FDateTime::Now();
    
    ShowOnScreenMessage(TEXT("Analyzing audio..."), 2.0f);
    
    ResourceAnalyzer->AnalyzeAudio(bFilterByVisible);
    
    bAnalysisInProgress = false;
    ShowOnScreenMessage(TEXT("Audio analysis completed!"), 3.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnAnalyzeLighting()
{
    if (!ResourceAnalyzer || bAnalysisInProgress)
    {
        return;
    }
    
    bAnalysisInProgress = true;
    LastAnalysisType = TEXT("Lighting");
    LastAnalysisTime = FDateTime::Now();
    
    ShowOnScreenMessage(TEXT("Analyzing lighting..."), 2.0f);
    
    ResourceAnalyzer->AnalyzeLighting(bFilterByVisible);
    
    bAnalysisInProgress = false;
    ShowOnScreenMessage(TEXT("Lighting analysis completed!"), 3.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnAnalyzePostProcess()
{
    if (!ResourceAnalyzer || bAnalysisInProgress)
    {
        return;
    }
    
    bAnalysisInProgress = true;
    LastAnalysisType = TEXT("PostProcess");  // Changed to no dash
    LastAnalysisTime = FDateTime::Now();
    
    ShowOnScreenMessage(TEXT("Analyzing post-process effects..."), 2.0f);
    
    ResourceAnalyzer->AnalyzePostProcessEffects(bFilterByVisible);
    
    bAnalysisInProgress = false;
    ShowOnScreenMessage(TEXT("Post-process analysis completed!"), 3.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnClearResults()
{
    if (!ResourceAnalyzer)
    {
        return;
    }
    
    ResourceAnalyzer->ClearAnalysisResults();
    LastAnalysisType = TEXT("");
    CurrentDisplayText = TEXT("");
    
    ShowOnScreenMessage(TEXT("Analysis results cleared"), 2.0f);
    
    if (bAnalysisDisplayVisible)
    {
        UpdateAnalysisDisplay();
    }
}

void UOptiloggerSubsystem::OnRefreshAnalysis()
{
    if (!LastAnalysisType.IsEmpty())
    {
        ShowOnScreenMessage(FString::Printf(TEXT("Refreshing %s analysis..."), *LastAnalysisType), 2.0f);
        
        // Re-run the last analysis type
        if (LastAnalysisType == TEXT("CurrentLevel"))
        {
            OnAnalyzeCurrentLevel();
        }
        else if (LastAnalysisType == TEXT("StaticMeshes"))
        {
            OnAnalyzeStaticMeshes();
        }
        else if (LastAnalysisType == TEXT("SkeletalMeshes"))
        {
            OnAnalyzeSkeletalMeshes();
        }
        else if (LastAnalysisType == TEXT("Textures"))
        {
            OnAnalyzeTextures();
        }
        else if (LastAnalysisType == TEXT("Materials"))
        {
            OnAnalyzeMaterials();
        }
        else if (LastAnalysisType == TEXT("Animations"))
        {
            OnAnalyzeAnimations();
        }
        else if (LastAnalysisType == TEXT("Audio"))
        {
            OnAnalyzeAudio();
        }
        else if (LastAnalysisType == TEXT("Lighting"))
        {
            OnAnalyzeLighting();
        }
        else if (LastAnalysisType == TEXT("PostProcess"))
        {
            OnAnalyzePostProcess();
        }
    }
    else
    {
        ShowOnScreenMessage(TEXT("No previous analysis to refresh"), 2.0f);
    }
}

void UOptiloggerSubsystem::OnToggleDisplay()
{
    ToggleAnalysisDisplay();
}

void UOptiloggerSubsystem::TriggerAnalysis(const FString& AnalysisType)
{
    LastAnalysisType = AnalysisType;
    if (AnalysisType == TEXT("CurrentLevel"))
    {
        OnAnalyzeCurrentLevel();
    }
    else if (AnalysisType == TEXT("StaticMeshes"))
    {
        OnAnalyzeStaticMeshes();
    }
    else if (AnalysisType == TEXT("SkeletalMeshes"))
    {
        OnAnalyzeSkeletalMeshes();
    }
    else if (AnalysisType == TEXT("Textures"))
    {
        OnAnalyzeTextures();
    }
    else if (AnalysisType == TEXT("Materials"))
    {
        OnAnalyzeMaterials();
    }
    else if (AnalysisType == TEXT("Animations"))
    {
        OnAnalyzeAnimations();
    }
    else if (AnalysisType == TEXT("Audio"))
    {
        OnAnalyzeAudio();
    }
    else if (AnalysisType == TEXT("Lighting"))
    {
        OnAnalyzeLighting();
    }
    else if (AnalysisType == TEXT("PostProcess"))
    {
        OnAnalyzePostProcess();
    }
}

void UOptiloggerSubsystem::DisplayAnalysisResults()
{
    bAnalysisDisplayVisible = true;
    UpdateAnalysisDisplay();
}

void UOptiloggerSubsystem::HideAnalysisResults()
{
    bAnalysisDisplayVisible = false;
    CurrentDisplayText = TEXT("");
}

void UOptiloggerSubsystem::ToggleAnalysisDisplay()
{
    if (bAnalysisDisplayVisible)
    {
        HideAnalysisResults();
        ShowOnScreenMessage(TEXT("Analysis display hidden"), 2.0f);
    }
    else
    {
        DisplayAnalysisResults();
        ShowOnScreenMessage(TEXT("Analysis display shown"), 2.0f);
    }
}

bool UOptiloggerSubsystem::ExportCurrentAnalysis()
{
    if (!ResourceAnalyzer)
    {
        return false;
    }
    
    return ResourceAnalyzer->ExportAnalysisToJSON();
}

FString UOptiloggerSubsystem::GetAnalysisSummary() const
{
    if (!ResourceAnalyzer)
    {
        return TEXT("Resource analyzer not available");
    }
    
    return FormatAnalysisData();
}

void UOptiloggerSubsystem::UpdateAnalysisDisplay()
{
    if (!bAnalysisDisplayVisible)
    {
        return;
    }
    
    CurrentDisplayText = FormatAnalysisData();
    DisplayUpdateTime = FPlatformTime::Seconds();
}

FString UOptiloggerSubsystem::FormatAnalysisData() const
{
    if (!ResourceAnalyzer)
    {
        return TEXT("No analysis data available");
    }
    
    FString DisplayText = TEXT("=== OPTILOGGER ANALYSIS RESULTS ===\n\n");
    
    // Add last analysis info
    if (!LastAnalysisType.IsEmpty())
    {
        DisplayText += FString::Printf(TEXT("Last Analysis: %s\n"), *LastAnalysisType);
        DisplayText += FString::Printf(TEXT("Analysis Time: %s\n\n"), *LastAnalysisTime.ToString());
        FString FilterStr = bFilterByVisible ? TEXT("Visible only") : TEXT("All in level");
        DisplayText += FString::Printf(TEXT("Filter: %s\n\n"), *FilterStr);
    }
    
    // Add summary counts and details only for the last type
    float TotalMemoryMB = 0.0f;
    if (LastAnalysisType == TEXT("CurrentLevel"))
    {
        // Show all
        DisplayText += FString::Printf(TEXT("Static Meshes: %d\n"), ResourceAnalyzer->GetStaticMeshAnalysisResults().Num());
        DisplayText += FString::Printf(TEXT("Skeletal Meshes: %d\n"), ResourceAnalyzer->GetSkeletalMeshAnalysisResults().Num());
        DisplayText += FString::Printf(TEXT("Textures: %d\n"), ResourceAnalyzer->GetTextureAnalysisResults().Num());
        DisplayText += FString::Printf(TEXT("Materials: %d\n"), ResourceAnalyzer->GetMaterialAnalysisResults().Num());
        DisplayText += FString::Printf(TEXT("Animations: %d\n"), ResourceAnalyzer->GetAnimationAnalysisResults().Num());
        DisplayText += FString::Printf(TEXT("Audio Assets: %d\n"), ResourceAnalyzer->GetAudioAnalysisResults().Num());
        DisplayText += FString::Printf(TEXT("Lights: %d\n"), ResourceAnalyzer->GetLightingAnalysisResults().Num());
        DisplayText += FString::Printf(TEXT("Post-Process Volumes: %d\n\n"), ResourceAnalyzer->GetPostProcessAnalysisResults().Num());

        for (const FStaticMeshAnalysisData& Data : ResourceAnalyzer->GetStaticMeshAnalysisResults())
        {
            TotalMemoryMB += Data.EstimatedMemoryUsageMB;
        }
        
        for (const FSkeletalMeshAnalysisData& Data : ResourceAnalyzer->GetSkeletalMeshAnalysisResults())
        {
            TotalMemoryMB += Data.EstimatedMemoryUsageMB;
        }
        
        for (const FTextureAnalysisData& Data : ResourceAnalyzer->GetTextureAnalysisResults())
        {
            TotalMemoryMB += Data.EstimatedMemoryUsageMB;
        }
    } else if (LastAnalysisType == TEXT("StaticMeshes"))
    {
        DisplayText += FString::Printf(TEXT("Static Meshes: %d\n\n"), ResourceAnalyzer->GetStaticMeshAnalysisResults().Num());
        for (const FStaticMeshAnalysisData& Data : ResourceAnalyzer->GetStaticMeshAnalysisResults())
        {
            DisplayText += FString::Printf(TEXT("- %s: Verts %d, Tris %d, LODs %d, Mem %.2f MB\n"), *Data.AssetName, Data.VertexCount, Data.TriangleCount, Data.LODCount, Data.EstimatedMemoryUsageMB);
            TotalMemoryMB += Data.EstimatedMemoryUsageMB;
        }
    } else if (LastAnalysisType == TEXT("SkeletalMeshes"))
    {
        DisplayText += FString::Printf(TEXT("Skeletal Meshes: %d\n\n"), ResourceAnalyzer->GetSkeletalMeshAnalysisResults().Num());
        for (const FSkeletalMeshAnalysisData& Data : ResourceAnalyzer->GetSkeletalMeshAnalysisResults())
        {
            DisplayText += FString::Printf(TEXT("- %s: Bones %d, Verts %d, LODs %d, Mem %.2f MB\n"), *Data.AssetName, Data.BoneCount, Data.VertexCount, Data.LODCount, Data.EstimatedMemoryUsageMB);
            TotalMemoryMB += Data.EstimatedMemoryUsageMB;
        }
    } else if (LastAnalysisType == TEXT("Textures"))
    {
        DisplayText += FString::Printf(TEXT("Textures: %d\n\n"), ResourceAnalyzer->GetTextureAnalysisResults().Num());
        for (const FTextureAnalysisData& Data : ResourceAnalyzer->GetTextureAnalysisResults())
        {
            DisplayText += FString::Printf(TEXT("- %s: %dx%d, %s, Mem %.2f MB\n"), *Data.AssetName, Data.Width, Data.Height, *Data.CompressionFormat, Data.EstimatedMemoryUsageMB);
            TotalMemoryMB += Data.EstimatedMemoryUsageMB;
        }
    } else if (LastAnalysisType == TEXT("Materials"))
    {
        DisplayText += FString::Printf(TEXT("Materials: %d\n\n"), ResourceAnalyzer->GetMaterialAnalysisResults().Num());
        for (const FMaterialAnalysisData& Data : ResourceAnalyzer->GetMaterialAnalysisResults())
        {
            DisplayText += FString::Printf(TEXT("- %s: Textures %d, Total Inst %d, %s\n"), *Data.AssetName, Data.TextureReferences, Data.TotalShaderInstructions, *Data.ComplexityLevel);
        }
    } else if (LastAnalysisType == TEXT("Animations"))
    {
        DisplayText += FString::Printf(TEXT("Animations: %d\n\n"), ResourceAnalyzer->GetAnimationAnalysisResults().Num());
        for (const FAnimationAnalysisData& Data : ResourceAnalyzer->GetAnimationAnalysisResults())
        {
            DisplayText += FString::Printf(TEXT("- %s: Duration %.1fs, Keys %d, Mem %.2f MB\n"), *Data.AssetName, Data.Duration, Data.KeyframeCount, Data.EstimatedMemoryUsageMB);
            TotalMemoryMB += Data.EstimatedMemoryUsageMB;
        }
    } else if (LastAnalysisType == TEXT("Audio"))
    {
        DisplayText += FString::Printf(TEXT("Audio Assets: %d\n\n"), ResourceAnalyzer->GetAudioAnalysisResults().Num());
        for (const FAudioAnalysisData& Data : ResourceAnalyzer->GetAudioAnalysisResults())
        {
            DisplayText += FString::Printf(TEXT("- %s: Duration %.1fs, Sample %dHz, Mem %.2f MB\n"), *Data.AssetName, Data.Duration, Data.SampleRate, Data.EstimatedMemoryUsageMB);
            TotalMemoryMB += Data.EstimatedMemoryUsageMB;
        }
    } else if (LastAnalysisType == TEXT("Lighting"))
    {
        DisplayText += FString::Printf(TEXT("Lights: %d\n\n"), ResourceAnalyzer->GetLightingAnalysisResults().Num());
        for (const FLightingAnalysisData& Data : ResourceAnalyzer->GetLightingAnalysisResults())
        {
            DisplayText += FString::Printf(TEXT("- %s: Type %s, Mobility %s\n"), *Data.LightName, *Data.LightType, *Data.Mobility);
        }
    } else if (LastAnalysisType == TEXT("PostProcess"))
    {
        DisplayText += FString::Printf(TEXT("Post-Process Volumes: %d\n\n"), ResourceAnalyzer->GetPostProcessAnalysisResults().Num());
        for (const FPostProcessAnalysisData& Data : ResourceAnalyzer->GetPostProcessAnalysisResults())
        {
            DisplayText += FString::Printf(TEXT("- %s: Priority %.1f, Effects %d\n"), *Data.VolumeName, Data.Priority, Data.ActiveEffects.Num());
        }
    }

    DisplayText += FString::Printf(TEXT("Estimated Total Memory: %.2f MB\n\n"), TotalMemoryMB);
    
    // Add controls info
    DisplayText += TEXT("=== CONTROLS ===\n");
    DisplayText += TEXT("Ctrl+NumPad1: Analyze Current Level\n");
    DisplayText += TEXT("Ctrl+NumPad2: Export Report\n");
    DisplayText += TEXT("Ctrl+NumPad3: Analyze Static Meshes\n");
    DisplayText += TEXT("Ctrl+NumPad4: Analyze Skeletal Meshes\n");
    DisplayText += TEXT("Ctrl+NumPad5: Analyze Textures\n");
    DisplayText += TEXT("Ctrl+NumPad6: Analyze Materials\n");
    DisplayText += TEXT("Ctrl+NumPad7: Analyze Animations\n");
    DisplayText += TEXT("Ctrl+NumPad8: Analyze Audio\n");
    DisplayText += TEXT("Ctrl+NumPad9: Analyze Lighting\n");
    DisplayText += TEXT("Ctrl+NumPad0: Analyze Post-Process\n");
    DisplayText += TEXT("Ctrl+Delete: Clear Results\n");
    DisplayText += TEXT("F5: Refresh Analysis\n");
    DisplayText += TEXT("F6: Toggle Display\n");
    
    return DisplayText;
}

void UOptiloggerSubsystem::ShowOnScreenMessage(const FString& Message, float Duration)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Green, 
            FString::Printf(TEXT("Optilogger: %s"), *Message));
    }
}

void UOptiloggerSubsystem::SetFilterByVisible(bool bVisible)
{
    bFilterByVisible = bVisible;
    if (!LastAnalysisType.IsEmpty()) OnRefreshAnalysis();
}