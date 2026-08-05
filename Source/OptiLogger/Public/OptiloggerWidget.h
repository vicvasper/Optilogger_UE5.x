// Copyright (c) Victor Rivas Perez. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class ITableRow;
class STableViewBase;
class STextBlock;
class UOptiloggerSubsystem;
class UResourceAnalyzer;

/** One row in the results list. */
struct FAnalysisResultItem
{
	FString Name;
	FString Type;
	FString Details;
	float MemoryUsageMB = 0.0f;
	FLinearColor StatusColor = FLinearColor::White;

	FAnalysisResultItem(const FString& InName, const FString& InType, const FString& InDetails,
		float InMemoryUsageMB, const FLinearColor& InStatusColor)
		: Name(InName)
		, Type(InType)
		, Details(InDetails)
		, MemoryUsageMB(InMemoryUsageMB)
		, StatusColor(InStatusColor)
	{
	}
};

/**
 * The OptiLogger tab: analysis buttons, a running summary, and a sortable list of the assets
 * the last pass found. All work is delegated to UOptiloggerSubsystem; this widget only
 * presents it.
 */
class SOptiloggerWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOptiloggerWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Rebuilds the results list and summary from the analyzer's current state. */
	void RefreshDisplay();

	/** Runs an analysis by identifier and refreshes the UI. */
	void TriggerAnalysisAndRefreshUI(const FString& AnalysisType);

	/** Builds the cell widget for one column. Called by SAnalysisResultRow. */
	TSharedRef<SWidget> OnGenerateWidgetForColumn(TSharedPtr<FAnalysisResultItem> Item, const FName& ColumnId) const;

private:
	TSharedRef<SWidget> CreateToolbar();
	TSharedRef<SWidget> CreateSummaryPanel();
	TSharedRef<SWidget> CreateControlsPanel();
	TSharedRef<SWidget> CreateAnalysisResultsPanel();

	TSharedRef<ITableRow> OnGenerateRowForAnalysisResult(TSharedPtr<FAnalysisResultItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	 * Handler for every analysis button.
	 *
	 * Replaces nine methods whose entire body was one call differing only in a string literal;
	 * the analysis type is bound as a payload on the delegate instead.
	 */
	FReply OnAnalysisTypeClicked(FString AnalysisType);

	FReply OnExportReportClicked();
	FReply OnClearResultsClicked();
	FReply OnRefreshClicked();

	ECheckBoxState IsFilterChecked() const;
	void OnFilterChanged(ECheckBoxState NewState);

	void PopulateAnalysisResults();

	UOptiloggerSubsystem* GetOptiloggerSubsystem() const;
	UResourceAnalyzer* GetResourceAnalyzer() const;
	bool IsAnalyzerAvailable() const;

	FText GetAnalysisSummaryText() const;
	FLinearColor GetMemoryUsageColor(float MemoryUsageMB) const;
	FString FormatMemorySize(float MemoryMB) const;

	/**
	 * Weak, not raw.
	 *
	 * An SWidget is not a UObject, so a raw pointer stored here is invisible to the garbage
	 * collector: nothing keeps the subsystem alive and nothing clears the pointer when it goes.
	 */
	TWeakObjectPtr<UOptiloggerSubsystem> OptiloggerSubsystem;

	TSharedPtr<SListView<TSharedPtr<FAnalysisResultItem>>> AnalysisResultsListView;
	TSharedPtr<STextBlock> SummaryTextBlock;
	TSharedPtr<STextBlock> StatusTextBlock;

	TArray<TSharedPtr<FAnalysisResultItem>> AnalysisResultItems;

	FSlateFontInfo HeaderFont;
	FSlateFontInfo NormalFont;
	FSlateFontInfo SmallFont;
};

/** Multi-column row delegating its cells back to the owning widget. */
class SAnalysisResultRow : public SMultiColumnTableRow<TSharedPtr<FAnalysisResultItem>>
{
public:
	SLATE_BEGIN_ARGS(SAnalysisResultRow) {}
		SLATE_ARGUMENT(TSharedPtr<FAnalysisResultItem>, Item)
		SLATE_ARGUMENT(TWeakPtr<SOptiloggerWidget>, OwnerWidget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
	{
		Item = InArgs._Item;
		OwnerWidget = InArgs._OwnerWidget;

		SMultiColumnTableRow<TSharedPtr<FAnalysisResultItem>>::Construct(FTableRowArgs(), OwnerTable);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		// Weak: rows outlive their owner during teardown, and the previous raw back-pointer
		// was dereferenced without a check.
		const TSharedPtr<SOptiloggerWidget> Owner = OwnerWidget.Pin();
		return Owner.IsValid() ? Owner->OnGenerateWidgetForColumn(Item, ColumnName) : SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FAnalysisResultItem> Item;
	TWeakPtr<SOptiloggerWidget> OwnerWidget;
};
