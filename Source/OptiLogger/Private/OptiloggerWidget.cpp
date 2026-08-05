#include "OptiloggerWidget.h"
#include "OptiloggerSubsystem.h"
#include "ResourceAnalyzer.h"
#include "Editor.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "OptiloggerWidget"

namespace
{
	// -- Typography ------------------------------------------------------------------------
	constexpr int32 HeaderFontSize = 14;
	constexpr int32 NormalFontSize = 10;
	constexpr int32 SmallFontSize = 8;

	// -- Palette ---------------------------------------------------------------------------
	const FLinearColor HeaderColor(0.8f, 0.8f, 1.0f);
	const FLinearColor NormalTextColor(0.9f, 0.9f, 0.9f);
	const FLinearColor WarningColor(1.0f, 0.8f, 0.0f);
	const FLinearColor ErrorColor(1.0f, 0.3f, 0.3f);
	const FLinearColor SuccessColor(0.3f, 1.0f, 0.3f);

	// -- Memory reporting ------------------------------------------------------------------
	//
	// Thresholds at which a single asset's estimated footprint changes colour in the list.
	// Tuned for per-asset figures, not for a whole level.

	constexpr float MemoryColorLowMB = 1.0f;
	constexpr float MemoryColorModerateMB = 10.0f;
	constexpr float MemoryColorHighMB = 50.0f;

	/** Unit boundaries for FormatMemorySize; also the MB<->KB and MB<->GB conversion factor. */
	constexpr float KilobytesPerMegabyte = 1024.0f;
	constexpr float MegabytesPerGigabyte = 1024.0f;
}

void SOptiloggerWidget::Construct(const FArguments& InArgs)
{
	// Guarded: GEditor is null in a commandlet or during early startup, and the result was
	// dereferenced unconditionally a few lines below.
	OptiloggerSubsystem = GEditor ? GEditor->GetEditorSubsystem<UOptiloggerSubsystem>() : nullptr;

    HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", HeaderFontSize);
    NormalFont = FCoreStyle::GetDefaultFontStyle("Regular", NormalFontSize);
    SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", SmallFontSize);

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8.0f)
        [
            SNew(SVerticalBox)
            
            // Header
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 10)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("PluginTitle", "Optilogger - Resource Analysis Tool"))
                .Font(HeaderFont)
                .ColorAndOpacity(HeaderColor)
                .Justification(ETextJustify::Center)
            ]

            // Toolbar
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 10)
            [
                CreateToolbar()
            ]

            // Main content area
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal)

                // Left panel - Controls and Summary
                + SSplitter::Slot()
                .Value(0.3f)
                [
                    SNew(SVerticalBox)

                    // Summary Panel
                    + SVerticalBox::Slot()
                    .FillHeight(0.4f)
                    .Padding(0, 0, 5, 5)
                    [
                        CreateSummaryPanel()
                    ]

                    // Controls Panel
                    + SVerticalBox::Slot()
                    .FillHeight(0.6f)
                    .Padding(0, 5, 5, 0)
                    [
                        CreateControlsPanel()
                    ]
                ]

                // Right panel - Analysis Results
                + SSplitter::Slot()
                .Value(0.7f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    .Padding(5, 0, 0, 0)
                    [
                        CreateAnalysisResultsPanel()
                    ]
                ]
            ]

            // Status bar
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 10, 0, 0)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                .Padding(5.0f)
                [
                    SAssignNew(StatusTextBlock, STextBlock)
                    .Text(LOCTEXT("StatusReady", "Ready"))
                    .Font(SmallFont)
                    .ColorAndOpacity(NormalTextColor)
                ]
            ]
        ]
    ];

	// Shows whatever the analyzer already holds. Opening the tab no longer kicks off a full
	// level analysis: that pass is synchronous and blocks the editor, which is a surprising
	// cost for merely opening a panel. The user presses "Analyze Level" when they want it.
	RefreshDisplay();
}

void SOptiloggerWidget::TriggerAnalysisAndRefreshUI(const FString& AnalysisType)
{
	UOptiloggerSubsystem* Subsystem = GetOptiloggerSubsystem();
	if (!Subsystem)
	{
		return;
	}

	// The analysis runs synchronously on this thread, so no frame is presented while it works
	// and an "Analyzing..." message set here would never appear. Only the outcome is reported.
	Subsystem->TriggerAnalysis(AnalysisType);
	RefreshDisplay();

	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(FText::Format(
			LOCTEXT("AnalysisComplete", "Analyzed {0}"), FText::FromString(AnalysisType)));
	}
}

FReply SOptiloggerWidget::OnAnalysisTypeClicked(FString AnalysisType)
{
	TriggerAnalysisAndRefreshUI(AnalysisType);
	return FReply::Handled();
}

TSharedRef<SWidget> SOptiloggerWidget::CreateToolbar()
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(2)
        [
            SNew(SButton)
            .Text(LOCTEXT("AnalyzeLevel", "Analyze Level"))
            .ToolTipText(LOCTEXT("AnalyzeLevelTooltip", "Analyze all resources in the current level (Ctrl+NumPad1)"))
            .OnClicked(FOnClicked::CreateSP(this, &SOptiloggerWidget::OnAnalysisTypeClicked, FString(TEXT("CurrentLevel"))))
            .IsEnabled(this, &SOptiloggerWidget::IsAnalyzerAvailable)
        ]

        + SHorizontalBox::Slot().AutoWidth().Padding(2)
        [
            SNew(SButton)
            .Text(LOCTEXT("ExportReport", "Export Report"))
            .ToolTipText(LOCTEXT("ExportReportTooltip", "Export analysis results to JSON file (Ctrl+NumPad2)"))
            .OnClicked(this, &SOptiloggerWidget::OnExportReportClicked)
            .IsEnabled(this, &SOptiloggerWidget::IsAnalyzerAvailable)
        ]

        + SHorizontalBox::Slot().AutoWidth().Padding(2)
        [
            SNew(SButton)
            .Text(LOCTEXT("ClearResults", "Clear"))
            .ToolTipText(LOCTEXT("ClearResultsTooltip", "Clear all analysis results (Ctrl+Delete)"))
            .OnClicked(this, &SOptiloggerWidget::OnClearResultsClicked)
            .IsEnabled(this, &SOptiloggerWidget::IsAnalyzerAvailable)
        ]
        
        + SHorizontalBox::Slot().AutoWidth().Padding(2)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SOptiloggerWidget::IsFilterChecked)
            .OnCheckStateChanged(this, &SOptiloggerWidget::OnFilterChanged)
            .Content()
            [
                SNew(STextBlock).Text(LOCTEXT("FilterVisible", "Visible Only"))
            ]
        ];
}

TSharedRef<SWidget> SOptiloggerWidget::CreateSummaryPanel()
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 5)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SummaryTitle", "Analysis Summary"))
                .Font(HeaderFont)
                .ColorAndOpacity(HeaderColor)
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SScrollBox)

                + SScrollBox::Slot()
                [
                    SAssignNew(SummaryTextBlock, STextBlock)
                        .Text_Lambda([this]() { return GetAnalysisSummaryText(); })
                        .Font(NormalFont)
                        .ColorAndOpacity(NormalTextColor)
                        .AutoWrapText(true)
                ]
            ]
        ];
}

TSharedRef<SWidget> SOptiloggerWidget::CreateControlsPanel()
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 5)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ControlsTitle", "Analysis Controls"))
                .Font(HeaderFont)
                .ColorAndOpacity(HeaderColor)
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SScrollBox)

                + SScrollBox::Slot()
                [
                    SNew(SUniformGridPanel)
                    .SlotPadding(2.0f)

                    // Row 1
                    + SUniformGridPanel::Slot(0, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("StaticMeshes", "Static Meshes"))
                        .ToolTipText(LOCTEXT("StaticMeshesTooltip", "Analyze static mesh resources (Ctrl+NumPad3)"))
                        .OnClicked(FOnClicked::CreateSP(this, &SOptiloggerWidget::OnAnalysisTypeClicked, FString(TEXT("StaticMeshes"))))
                        .HAlign(HAlign_Fill)
                    ]

                    + SUniformGridPanel::Slot(1, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("SkeletalMeshes", "Skeletal Meshes"))
                        .ToolTipText(LOCTEXT("SkeletalMeshesTooltip", "Analyze skeletal mesh resources (Ctrl+NumPad4)"))
                        .OnClicked(FOnClicked::CreateSP(this, &SOptiloggerWidget::OnAnalysisTypeClicked, FString(TEXT("SkeletalMeshes"))))
                        .HAlign(HAlign_Fill)
                    ]

                    // Row 2
                    + SUniformGridPanel::Slot(0, 1)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Textures", "Textures"))
                        .ToolTipText(LOCTEXT("TexturesTooltip", "Analyze texture resources (Ctrl+NumPad5)"))
                        .OnClicked(FOnClicked::CreateSP(this, &SOptiloggerWidget::OnAnalysisTypeClicked, FString(TEXT("Textures"))))
                        .HAlign(HAlign_Fill)
                    ]

                    + SUniformGridPanel::Slot(1, 1)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Materials", "Materials"))
                        .ToolTipText(LOCTEXT("MaterialsTooltip", "Analyze material resources (Ctrl+NumPad6)"))
                        .OnClicked(FOnClicked::CreateSP(this, &SOptiloggerWidget::OnAnalysisTypeClicked, FString(TEXT("Materials"))))
                        .HAlign(HAlign_Fill)
                    ]

                    // Row 3
                    + SUniformGridPanel::Slot(0, 2)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Animations", "Animations"))
                        .ToolTipText(LOCTEXT("AnimationsTooltip", "Analyze animation resources (Ctrl+NumPad7)"))
                        .OnClicked(FOnClicked::CreateSP(this, &SOptiloggerWidget::OnAnalysisTypeClicked, FString(TEXT("Animations"))))
                        .HAlign(HAlign_Fill)
                    ]

                    + SUniformGridPanel::Slot(1, 2)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Audio", "Audio"))
                        .ToolTipText(LOCTEXT("AudioTooltip", "Analyze audio resources (Ctrl+NumPad8)"))
                        .OnClicked(FOnClicked::CreateSP(this, &SOptiloggerWidget::OnAnalysisTypeClicked, FString(TEXT("Audio"))))
                        .HAlign(HAlign_Fill)
                    ]

                    // Row 4
                    + SUniformGridPanel::Slot(0, 3)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Lighting", "Lighting"))
                        .ToolTipText(LOCTEXT("LightingTooltip", "Analyze lighting resources (Ctrl+NumPad9)"))
                        .OnClicked(FOnClicked::CreateSP(this, &SOptiloggerWidget::OnAnalysisTypeClicked, FString(TEXT("Lighting"))))
                        .HAlign(HAlign_Fill)
                    ]

                    + SUniformGridPanel::Slot(1, 3)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("PostProcess", "Post-Process"))
                        .ToolTipText(LOCTEXT("PostProcessTooltip", "Analyze post-process effects (Ctrl+NumPad0)"))
                        .OnClicked(FOnClicked::CreateSP(this, &SOptiloggerWidget::OnAnalysisTypeClicked, FString(TEXT("PostProcess"))))
                        .HAlign(HAlign_Fill)
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> SOptiloggerWidget::CreateAnalysisResultsPanel()
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 5)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ResultsTitle", "Analysis Results"))
                .Font(HeaderFont)
                .ColorAndOpacity(HeaderColor)
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(AnalysisResultsListView, SListView<TSharedPtr<FAnalysisResultItem>>)
                .ListItemsSource(&AnalysisResultItems)
                .OnGenerateRow(this, &SOptiloggerWidget::OnGenerateRowForAnalysisResult)
                .HeaderRow
                (
                    SNew(SHeaderRow)
          
                    + SHeaderRow::Column("Name")
                        .DefaultLabel(LOCTEXT("NameColumn", "Asset Name"))
                        .FillWidth(0.4f)
          
                    + SHeaderRow::Column("Type")
                        .DefaultLabel(LOCTEXT("TypeColumn", "Type"))
                        .FillWidth(0.2f)
          
                    + SHeaderRow::Column("Memory")
                        .DefaultLabel(LOCTEXT("MemoryColumn", "Memory (MB)"))
                        .FillWidth(0.2f)
          
                    + SHeaderRow::Column("Details")
                        .DefaultLabel(LOCTEXT("DetailsColumn", "Details"))
                        .FillWidth(0.2f)
                )
            ]
        ];
}

FReply SOptiloggerWidget::OnExportReportClicked()
{
    if (UOptiloggerSubsystem* Subsystem = GetOptiloggerSubsystem())
    {
        bool bSuccess = Subsystem->ExportCurrentAnalysis();
        if (bSuccess)
        {
            StatusTextBlock->SetText(LOCTEXT("ExportSuccess", "Analysis report exported successfully"));
        }
        else
        {
            StatusTextBlock->SetText(LOCTEXT("ExportFailed", "Failed to export analysis report"));
        }
    }
    return FReply::Handled();
}

FReply SOptiloggerWidget::OnClearResultsClicked()
{
    if (UResourceAnalyzer* Analyzer = GetResourceAnalyzer())
    {
        Analyzer->ClearAnalysisResults();
        if (UOptiloggerSubsystem* Subsystem = GetOptiloggerSubsystem())
        {
            // Was reaching in and assigning the subsystem's public LastAnalysisType field.
            Subsystem->ClearAnalysisState();
        }
        RefreshDisplay();
        StatusTextBlock->SetText(LOCTEXT("ResultsCleared", "Analysis results cleared"));
    }
    return FReply::Handled();
}

FReply SOptiloggerWidget::OnRefreshClicked()
{
    if (UOptiloggerSubsystem* Subsystem = GetOptiloggerSubsystem())
    {
        FString LastAnalysis = Subsystem->GetLastAnalysisType();
        if (!LastAnalysis.IsEmpty())
        {
            TriggerAnalysisAndRefreshUI(LastAnalysis);
            StatusTextBlock->SetText(LOCTEXT("ResultsRefreshed", "Analysis results refreshed"));
        }
        else
        {
            StatusTextBlock->SetText(LOCTEXT("NothingToRefresh", "No previous analysis to refresh."));
        }
    }
    return FReply::Handled();
}


TSharedRef<ITableRow> SOptiloggerWidget::OnGenerateRowForAnalysisResult(
    TSharedPtr<FAnalysisResultItem> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    // Ahora usamos nuestra clase de fila personalizada:
    return SNew(SAnalysisResultRow, OwnerTable)
        .Item(Item)
        .OwnerWidget(SharedThis(this));
}

TSharedRef<SWidget> SOptiloggerWidget::OnGenerateWidgetForColumn(
    TSharedPtr<FAnalysisResultItem> Item,
    const FName& ColumnId) const
{
    if (ColumnId == "Name")
    {
        return SNew(STextBlock)
            .Text(FText::FromString(Item->Name))
            .Font(NormalFont)
            .ColorAndOpacity(Item->StatusColor);
    }
    else if (ColumnId == "Type")
    {
        return SNew(STextBlock)
            .Text(FText::FromString(Item->Type))
            .Font(NormalFont);
    }
    else if (ColumnId == "Memory")
    {
        return SNew(STextBlock)
            .Text(FText::FromString(FormatMemorySize(Item->MemoryUsageMB)))
            .Font(NormalFont)
            .ColorAndOpacity(GetMemoryUsageColor(Item->MemoryUsageMB));
    }
    else if (ColumnId == "Details")
    {
        return SNew(STextBlock)
            .Text(FText::FromString(Item->Details))
            .Font(SmallFont);
    }
    
    return SNew(STextBlock);
}

void SOptiloggerWidget::PopulateAnalysisResults()
{
    AnalysisResultItems.Empty();
    
    UResourceAnalyzer* Analyzer = GetResourceAnalyzer();
    UOptiloggerSubsystem* Subsystem = GetOptiloggerSubsystem();
    if (!Analyzer || !Subsystem)
    {
        if (AnalysisResultsListView.IsValid())
        {
            AnalysisResultsListView->RequestListRefresh();
        }
        return;
    }
    
    const FString AnalysisTypeToShow = Subsystem->GetLastAnalysisType();
    const bool bShowAll = AnalysisTypeToShow.IsEmpty() || AnalysisTypeToShow == TEXT("CurrentLevel");

    if (bShowAll || AnalysisTypeToShow == TEXT("StaticMeshes"))
    {
        for (const FStaticMeshAnalysisData& Data : Analyzer->GetStaticMeshAnalysisResults())
        {
            FString Details = FString::Printf(TEXT("Verts: %d, Tris: %d, LODs: %d"),
                Data.VertexCount, Data.TriangleCount, Data.LODCount);
            
            AnalysisResultItems.Add(MakeShareable(new FAnalysisResultItem(
                Data.AssetName,
                TEXT("Static Mesh"),
                Details,
                Data.EstimatedMemoryUsageMB,
                NormalTextColor
            )));
        }
    }

    if (bShowAll || AnalysisTypeToShow == TEXT("SkeletalMeshes"))
    {
        for (const FSkeletalMeshAnalysisData& Data : Analyzer->GetSkeletalMeshAnalysisResults())
        {
            FString Details = FString::Printf(TEXT("Bones: %d, Verts: %d, LODs: %d"),
                Data.BoneCount, Data.VertexCount, Data.LODCount);
            
            AnalysisResultItems.Add(MakeShareable(new FAnalysisResultItem(
                Data.AssetName,
                TEXT("Skeletal Mesh"),
                Details,
                Data.EstimatedMemoryUsageMB,
                NormalTextColor
            )));
        }
    }

    if (bShowAll || AnalysisTypeToShow == TEXT("Textures"))
    {
        for (const FTextureAnalysisData& Data : Analyzer->GetTextureAnalysisResults())
        {
            FString Details = FString::Printf(TEXT("%dx%d, %s"),
                Data.Width, Data.Height, *Data.CompressionFormat);
            
            AnalysisResultItems.Add(MakeShareable(new FAnalysisResultItem(
                Data.AssetName,
                TEXT("Texture"),
                Details,
                Data.EstimatedMemoryUsageMB,
                NormalTextColor
            )));
        }
    }
    
    if (bShowAll || AnalysisTypeToShow == TEXT("Materials"))
    {
        for (const FMaterialAnalysisData& Data : Analyzer->GetMaterialAnalysisResults())
        {
            FString Details = FString::Printf(
                TEXT("Textures: %d, VS: %d, PS: %d, Total: %d, %s"),
                Data.TextureReferences,
                Data.NumVertexShaderInstructions,
                Data.NumPixelShaderInstructions,
                Data.TotalShaderInstructions,
                *Data.ComplexityLevel
            );
        
            FLinearColor StatusColor = (Data.TotalShaderInstructions > 300) ? WarningColor : NormalTextColor;

            AnalysisResultItems.Add(MakeShareable(new FAnalysisResultItem(
                Data.AssetName,
                TEXT("Material"),
                Details,
                0.0f,
                StatusColor
            )));
        }
    }

    if (bShowAll || AnalysisTypeToShow == TEXT("Animations"))
    {
        for (const FAnimationAnalysisData& Data : Analyzer->GetAnimationAnalysisResults())
        {
            FString Details = FString::Printf(TEXT("%.1fs, %d keys"),
                Data.Duration, Data.KeyframeCount);
            
            AnalysisResultItems.Add(MakeShareable(new FAnalysisResultItem(
                Data.AssetName,
                TEXT("Animation"),
                Details,
                Data.EstimatedMemoryUsageMB,
                NormalTextColor
            )));
        }
    }

    if (bShowAll || AnalysisTypeToShow == TEXT("Audio"))
    {
        for (const FAudioAnalysisData& Data : Analyzer->GetAudioAnalysisResults())
        {
            FString Details = FString::Printf(TEXT("%.1fs, %dHz"),
                Data.Duration, Data.SampleRate);
            
            AnalysisResultItems.Add(MakeShareable(new FAnalysisResultItem(
                Data.AssetName,
                TEXT("Audio"),
                Details,
                Data.EstimatedMemoryUsageMB,
                NormalTextColor
            )));
        }
    }

    if (bShowAll || AnalysisTypeToShow == TEXT("Lighting"))
    {
        for (const FLightingAnalysisData& Data : Analyzer->GetLightingAnalysisResults())
        {
            FString Details = FString::Printf(TEXT("%s, %s"),
                *Data.LightType, *Data.Mobility);
            
            AnalysisResultItems.Add(MakeShareable(new FAnalysisResultItem(
                Data.LightName,
                TEXT("Light"),
                Details,
                0.0f,
                NormalTextColor
            )));
        }
    }

    if (bShowAll || AnalysisTypeToShow == TEXT("PostProcess"))
    {
        for (const FPostProcessAnalysisData& Data : Analyzer->GetPostProcessAnalysisResults())
        {
            FString Details = FString::Printf(TEXT("Effects: %d, Priority: %.1f"),
                Data.ActiveEffects.Num(), Data.Priority);
            
            AnalysisResultItems.Add(MakeShareable(new FAnalysisResultItem(
                Data.VolumeName,
                TEXT("Post-Process"),
                Details,
                0.0f,
                NormalTextColor
            )));
        }
    }

    if (AnalysisResultsListView.IsValid())
    {
        AnalysisResultsListView->RebuildList();
    }
}

void SOptiloggerWidget::RefreshDisplay()
{
    PopulateAnalysisResults();
    if (SummaryTextBlock.IsValid())
    {
        SummaryTextBlock->SetText(GetAnalysisSummaryText());
    }
}


UOptiloggerSubsystem* SOptiloggerWidget::GetOptiloggerSubsystem() const
{
	// Resolved from the weak handle cached in Construct rather than re-queried every call.
	// The previous version guarded on GEngine and then dereferenced GEditor.
	if (UOptiloggerSubsystem* Cached = OptiloggerSubsystem.Get())
	{
		return Cached;
	}

	return GEditor ? GEditor->GetEditorSubsystem<UOptiloggerSubsystem>() : nullptr;
}

UResourceAnalyzer* SOptiloggerWidget::GetResourceAnalyzer() const
{
    if (UOptiloggerSubsystem* Subsystem = GetOptiloggerSubsystem())
    {
        return Subsystem->GetResourceAnalyzer();
    }
    return nullptr;
}

bool SOptiloggerWidget::IsAnalyzerAvailable() const
{
	return GetResourceAnalyzer() != nullptr;
}

FText SOptiloggerWidget::GetAnalysisSummaryText() const
{
    UResourceAnalyzer* Analyzer = GetResourceAnalyzer();
    if (!Analyzer)
    {
        return LOCTEXT("NoAnalyzer", "Resource analyzer not available");
    }

    FString SummaryText = TEXT("Analysis Summary:\n\n");
    
    int32 StaticMeshCount = Analyzer->GetStaticMeshAnalysisResults().Num();
    int32 SkeletalMeshCount = Analyzer->GetSkeletalMeshAnalysisResults().Num();
    int32 TextureCount = Analyzer->GetTextureAnalysisResults().Num();
    int32 MaterialCount = Analyzer->GetMaterialAnalysisResults().Num();
    int32 AnimationCount = Analyzer->GetAnimationAnalysisResults().Num();
    int32 AudioCount = Analyzer->GetAudioAnalysisResults().Num();
    int32 LightCount = Analyzer->GetLightingAnalysisResults().Num();
    int32 PostProcessCount = Analyzer->GetPostProcessAnalysisResults().Num();

    SummaryText += FString::Printf(TEXT("Static Meshes: %d\n"), StaticMeshCount);
    SummaryText += FString::Printf(TEXT("Skeletal Meshes: %d\n"), SkeletalMeshCount);
    SummaryText += FString::Printf(TEXT("Textures: %d\n"), TextureCount);
    SummaryText += FString::Printf(TEXT("Materials: %d\n"), MaterialCount);
    SummaryText += FString::Printf(TEXT("Animations: %d\n"), AnimationCount);
    SummaryText += FString::Printf(TEXT("Audio Assets: %d\n"), AudioCount);
    SummaryText += FString::Printf(TEXT("Lights: %d\n"), LightCount);
    SummaryText += FString::Printf(TEXT("Post-Process: %d\n\n"), PostProcessCount);

    float TotalMemoryMB = 0.0f;
    
    for (const FStaticMeshAnalysisData& Data : Analyzer->GetStaticMeshAnalysisResults())
    {
        TotalMemoryMB += Data.EstimatedMemoryUsageMB;
    }
    
    for (const FSkeletalMeshAnalysisData& Data : Analyzer->GetSkeletalMeshAnalysisResults())
    {
        TotalMemoryMB += Data.EstimatedMemoryUsageMB;
    }
    
    for (const FTextureAnalysisData& Data : Analyzer->GetTextureAnalysisResults())
    {
        TotalMemoryMB += Data.EstimatedMemoryUsageMB;
    }
    
    for (const FAnimationAnalysisData& Data : Analyzer->GetAnimationAnalysisResults())
    {
        TotalMemoryMB += Data.EstimatedMemoryUsageMB;
    }
    
    for (const FAudioAnalysisData& Data : Analyzer->GetAudioAnalysisResults())
    {
        TotalMemoryMB += Data.EstimatedMemoryUsageMB;
    }

    SummaryText += FString::Printf(TEXT("Total Memory: %s"), *FormatMemorySize(TotalMemoryMB));

    return FText::FromString(SummaryText);
}

FLinearColor SOptiloggerWidget::GetMemoryUsageColor(float MemoryUsageMB) const
{
	if (MemoryUsageMB < MemoryColorLowMB)
	{
		return SuccessColor;
	}
	if (MemoryUsageMB < MemoryColorModerateMB)
	{
		return NormalTextColor;
	}
	if (MemoryUsageMB < MemoryColorHighMB)
	{
		return WarningColor;
	}
	return ErrorColor;
}

FString SOptiloggerWidget::FormatMemorySize(float MemoryMB) const
{
	if (MemoryMB < 1.0f)
	{
		return FString::Printf(TEXT("%.1f KB"), MemoryMB * KilobytesPerMegabyte);
	}
	if (MemoryMB < MegabytesPerGigabyte)
	{
		return FString::Printf(TEXT("%.1f MB"), MemoryMB);
	}
	return FString::Printf(TEXT("%.2f GB"), MemoryMB / MegabytesPerGigabyte);
}

ECheckBoxState SOptiloggerWidget::IsFilterChecked() const
{
	if (const UOptiloggerSubsystem* Subsystem = GetOptiloggerSubsystem())
	{
		return Subsystem->IsFilterByVisible() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void SOptiloggerWidget::OnFilterChanged(ECheckBoxState NewState)
{
	if (UOptiloggerSubsystem* Subsystem = GetOptiloggerSubsystem())
	{
		// SetFilterByVisible already re-runs the last analysis under the new filter. The
		// OnRefreshClicked() that followed ran the whole pass a second time on every toggle.
		Subsystem->SetFilterByVisible(NewState == ECheckBoxState::Checked);
		RefreshDisplay();
	}
}


#undef LOCTEXT_NAMESPACE