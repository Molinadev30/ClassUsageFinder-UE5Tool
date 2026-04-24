// Copyright (c) Ruben. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/SHeaderRow.h"
#include "ClassUsageScanner.h"

class SClassUsageFinderWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClassUsageFinderWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	using FMatchItem = TSharedPtr<FClassUsageMatch>;

	TWeakObjectPtr<UClass> TargetClass;
	TArray<FMatchItem> Results;
	TSharedPtr<SListView<FMatchItem>> ResultsList;

	bool bIncludeSubclasses = true;
	bool bScanDataAssets = true;
	bool bScanBlueprints = true;
	bool bResolveSoftRefs = true;

	FText GetTargetClassText() const;
	FReply OnPickClassClicked();
	FReply OnScanClicked();
	FReply OnClearClicked();
	FReply OnExportCsvClicked();

	TSharedRef<ITableRow> GenerateRow(FMatchItem Item, const TSharedRef<STableViewBase>& Owner);
	void OnRowDoubleClick(FMatchItem Item);
	void OpenAssetForItem(FMatchItem Item) const;
	void BrowseToAssetForItem(FMatchItem Item) const;

	ECheckBoxState GetIncludeSubclassesChecked() const;
	void OnIncludeSubclassesChanged(ECheckBoxState NewState);
	ECheckBoxState GetScanDataAssetsChecked() const;
	void OnScanDataAssetsChanged(ECheckBoxState NewState);
	ECheckBoxState GetScanBlueprintsChecked() const;
	void OnScanBlueprintsChanged(ECheckBoxState NewState);
	ECheckBoxState GetResolveSoftRefsChecked() const;
	void OnResolveSoftRefsChanged(ECheckBoxState NewState);

	FText GetResultsSummaryText() const;
};

class SClassUsageMatchRow : public SMultiColumnTableRow<TSharedPtr<FClassUsageMatch>>
{
public:
	SLATE_BEGIN_ARGS(SClassUsageMatchRow) {}
		SLATE_ARGUMENT(TSharedPtr<FClassUsageMatch>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FClassUsageMatch> Item;
};
