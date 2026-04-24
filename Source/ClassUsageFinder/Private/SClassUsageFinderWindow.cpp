// Copyright (c) Ruben. Licensed under the MIT License.

#include "SClassUsageFinderWindow.h"

#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "UObject/SoftObjectPath.h"

#define LOCTEXT_NAMESPACE "ClassUsageFinder"

namespace ClassUsageFinderUI
{
	static const FName ColumnAsset(TEXT("Asset"));
	static const FName ColumnAssetClass(TEXT("AssetClass"));
	static const FName ColumnProperty(TEXT("Property"));
	static const FName ColumnMatched(TEXT("Matched"));
	static const FName ColumnRefKind(TEXT("RefKind"));
	static const FName ColumnActions(TEXT("Actions"));
}

void SClassUsageFinderWindow::Construct(const FArguments& InArgs)
{
	using namespace ClassUsageFinderUI;

	ResultsList = SNew(SListView<FMatchItem>)
		.ListItemsSource(&Results)
		.OnGenerateRow(this, &SClassUsageFinderWindow::GenerateRow)
		.OnMouseButtonDoubleClick(this, &SClassUsageFinderWindow::OnRowDoubleClick)
		.SelectionMode(ESelectionMode::Single)
		.HeaderRow(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnAsset).DefaultLabel(LOCTEXT("ColAsset", "Asset")).FillWidth(0.28f)
			+ SHeaderRow::Column(ColumnAssetClass).DefaultLabel(LOCTEXT("ColAssetClass", "Asset Class")).FillWidth(0.16f)
			+ SHeaderRow::Column(ColumnProperty).DefaultLabel(LOCTEXT("ColProperty", "Property Path")).FillWidth(0.28f)
			+ SHeaderRow::Column(ColumnMatched).DefaultLabel(LOCTEXT("ColMatched", "Matched Class")).FillWidth(0.18f)
			+ SHeaderRow::Column(ColumnRefKind).DefaultLabel(LOCTEXT("ColRefKind", "Ref")).FixedWidth(60.f)
			+ SHeaderRow::Column(ColumnActions).DefaultLabel(LOCTEXT("ColActions", "Actions")).FixedWidth(110.f)
		);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(6)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
					[
						SNew(STextBlock).Text(LOCTEXT("TargetClassLabel", "Target C++ class:"))
					]

					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SButton)
						.OnClicked(this, &SClassUsageFinderWindow::OnPickClassClicked)
						.ToolTipText(LOCTEXT("PickClassTooltip", "Pick the C++ class to search for"))
						[
							SNew(STextBlock).Text(this, &SClassUsageFinderWindow::GetTargetClassText)
						]
					]

					+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ScanButton", "Scan Project"))
						.OnClicked(this, &SClassUsageFinderWindow::OnScanClicked)
					]

					+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ClearButton", "Clear"))
						.OnClicked(this, &SClassUsageFinderWindow::OnClearClicked)
					]

					+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ExportButton", "Export CSV"))
						.OnClicked(this, &SClassUsageFinderWindow::OnExportCsvClicked)
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SClassUsageFinderWindow::GetIncludeSubclassesChecked)
						.OnCheckStateChanged(this, &SClassUsageFinderWindow::OnIncludeSubclassesChanged)
						[ SNew(STextBlock).Text(LOCTEXT("OptSubclasses", "Include subclasses")) ]
					]

					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SClassUsageFinderWindow::GetScanDataAssetsChecked)
						.OnCheckStateChanged(this, &SClassUsageFinderWindow::OnScanDataAssetsChanged)
						[ SNew(STextBlock).Text(LOCTEXT("OptDataAssets", "Scan Data Assets")) ]
					]

					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SClassUsageFinderWindow::GetScanBlueprintsChecked)
						.OnCheckStateChanged(this, &SClassUsageFinderWindow::OnScanBlueprintsChanged)
						[ SNew(STextBlock).Text(LOCTEXT("OptBlueprints", "Scan Blueprints")) ]
					]

					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SCheckBox)
						.IsChecked(this, &SClassUsageFinderWindow::GetResolveSoftRefsChecked)
						.OnCheckStateChanged(this, &SClassUsageFinderWindow::OnResolveSoftRefsChanged)
						.ToolTipText(LOCTEXT("OptSoftRefsTip", "If enabled, soft class references will be loaded so subclass detection works on them."))
						[ SNew(STextBlock).Text(LOCTEXT("OptSoftRefs", "Resolve soft refs")) ]
					]
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(6, 0)
		[
			SNew(STextBlock)
			.Text(this, &SClassUsageFinderWindow::GetResultsSummaryText)
		]

		+ SVerticalBox::Slot().FillHeight(1.f).Padding(6)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				ResultsList.ToSharedRef()
			]
		]
	];
}

FText SClassUsageFinderWindow::GetTargetClassText() const
{
	if (UClass* C = TargetClass.Get())
	{
		return FText::FromString(C->GetName());
	}
	return LOCTEXT("NoClassPicked", "<pick a class>");
}

class FClassUsageFinderClassFilter : public IClassViewerFilter
{
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& /*Options*/, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> /*Funcs*/) override
	{
		return InClass != nullptr;
	}
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& /*Options*/, const TSharedRef<const IUnloadedBlueprintData> /*InUnloadedClassData*/, TSharedRef<FClassViewerFilterFuncs> /*Funcs*/) override
	{
		return false;
	}
};

FReply SClassUsageFinderWindow::OnPickClassClicked()
{
	FClassViewerModule& ClassViewer = FModuleManager::LoadModuleChecked<FClassViewerModule>(TEXT("ClassViewer"));

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	Options.bShowNoneOption = false;
	Options.bShowUnloadedBlueprints = false;
	Options.bShowObjectRootClass = true;
	Options.ClassFilters.Add(MakeShared<FClassUsageFinderClassFilter>());

	UClass* Picked = nullptr;
	const bool bPicked = SClassPickerDialog::PickClass(
		LOCTEXT("PickClassTitle", "Pick Class To Search For"),
		Options, Picked, UObject::StaticClass());

	if (bPicked && Picked)
	{
		TargetClass = Picked;
	}
	return FReply::Handled();
}

FReply SClassUsageFinderWindow::OnScanClicked()
{
	if (!TargetClass.IsValid())
	{
		FNotificationInfo Info(LOCTEXT("NoClassMsg", "Pick a class first."));
		Info.ExpireDuration = 3.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FReply::Handled();
	}

	FClassUsageScanOptions Opts;
	Opts.TargetClass = TargetClass;
	Opts.bIncludeSubclasses = bIncludeSubclasses;
	Opts.bScanDataAssets = bScanDataAssets;
	Opts.bScanBlueprints = bScanBlueprints;
	Opts.bResolveSoftRefs = bResolveSoftRefs;

	const TArray<FClassUsageMatch> Matches = FClassUsageScanner::ScanProject(Opts);

	Results.Reset();
	Results.Reserve(Matches.Num());
	for (const FClassUsageMatch& M : Matches)
	{
		Results.Add(MakeShared<FClassUsageMatch>(M));
	}

	if (ResultsList.IsValid())
	{
		ResultsList->RequestListRefresh();
	}

	FNotificationInfo Info(FText::Format(LOCTEXT("ScanDoneFmt", "Scan finished — {0} match(es) found."), FText::AsNumber(Matches.Num())));
	Info.ExpireDuration = 4.f;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

FReply SClassUsageFinderWindow::OnClearClicked()
{
	Results.Reset();
	if (ResultsList.IsValid()) ResultsList->RequestListRefresh();
	return FReply::Handled();
}

FReply SClassUsageFinderWindow::OnExportCsvClicked()
{
	if (Results.Num() == 0) return FReply::Handled();

	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop) return FReply::Handled();

	TArray<FString> OutFiles;
	const void* ParentHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const bool bOk = Desktop->SaveFileDialog(
		ParentHandle,
		TEXT("Export Class Usage Results"),
		FPaths::ProjectSavedDir(),
		TEXT("ClassUsage.csv"),
		TEXT("CSV Files (*.csv)|*.csv"),
		EFileDialogFlags::None,
		OutFiles);

	if (!bOk || OutFiles.Num() == 0) return FReply::Handled();

	FString Csv;
	Csv += TEXT("Asset,AssetClass,Property,MatchedClass,Soft\n");
	for (const FMatchItem& Item : Results)
	{
		if (!Item.IsValid()) continue;
		Csv += FString::Printf(TEXT("\"%s\",\"%s\",\"%s\",\"%s\",%s\n"),
			*Item->AssetPath.ToString(),
			*Item->AssetClassPath.ToString(),
			*Item->PropertyPath,
			*Item->MatchedClassPath.ToString(),
			Item->bIsSoftReference ? TEXT("true") : TEXT("false"));
	}
	FFileHelper::SaveStringToFile(Csv, *OutFiles[0]);
	return FReply::Handled();
}

TSharedRef<ITableRow> SClassUsageFinderWindow::GenerateRow(FMatchItem Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SClassUsageMatchRow, Owner).Item(Item);
}

void SClassUsageFinderWindow::OnRowDoubleClick(FMatchItem Item)
{
	OpenAssetForItem(Item);
}

void SClassUsageFinderWindow::OpenAssetForItem(FMatchItem Item) const
{
	if (!Item.IsValid()) return;
	if (UObject* Asset = Item->AssetPath.TryLoad())
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
	}
}

void SClassUsageFinderWindow::BrowseToAssetForItem(FMatchItem Item) const
{
	if (!Item.IsValid()) return;
	UObject* Asset = Item->AssetPath.TryLoad();
	if (!Asset) return;
	FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<UObject*> SyncObjs;
	SyncObjs.Add(Asset);
	CB.Get().SyncBrowserToAssets(SyncObjs);
}

ECheckBoxState SClassUsageFinderWindow::GetIncludeSubclassesChecked() const { return bIncludeSubclasses ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
void SClassUsageFinderWindow::OnIncludeSubclassesChanged(ECheckBoxState NewState) { bIncludeSubclasses = (NewState == ECheckBoxState::Checked); }
ECheckBoxState SClassUsageFinderWindow::GetScanDataAssetsChecked() const { return bScanDataAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
void SClassUsageFinderWindow::OnScanDataAssetsChanged(ECheckBoxState NewState) { bScanDataAssets = (NewState == ECheckBoxState::Checked); }
ECheckBoxState SClassUsageFinderWindow::GetScanBlueprintsChecked() const { return bScanBlueprints ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
void SClassUsageFinderWindow::OnScanBlueprintsChanged(ECheckBoxState NewState) { bScanBlueprints = (NewState == ECheckBoxState::Checked); }
ECheckBoxState SClassUsageFinderWindow::GetResolveSoftRefsChecked() const { return bResolveSoftRefs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
void SClassUsageFinderWindow::OnResolveSoftRefsChanged(ECheckBoxState NewState) { bResolveSoftRefs = (NewState == ECheckBoxState::Checked); }

FText SClassUsageFinderWindow::GetResultsSummaryText() const
{
	if (Results.Num() == 0)
	{
		return LOCTEXT("NoResults", "No results yet. Pick a class and press Scan.");
	}
	return FText::Format(LOCTEXT("ResultsCountFmt", "{0} reference(s) found."), FText::AsNumber(Results.Num()));
}

// ------------- Row widget -------------

void SClassUsageMatchRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
{
	Item = InArgs._Item;
	SMultiColumnTableRow<TSharedPtr<FClassUsageMatch>>::Construct(FSuperRowType::FArguments(), OwnerTable);
}

TSharedRef<SWidget> SClassUsageMatchRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	using namespace ClassUsageFinderUI;
	if (!Item.IsValid()) return SNullWidget::NullWidget;

	if (ColumnName == ColumnAsset)
	{
		return SNew(STextBlock).Text(FText::FromString(Item->AssetPath.GetAssetName()))
			.ToolTipText(FText::FromString(Item->AssetPath.ToString()));
	}
	if (ColumnName == ColumnAssetClass)
	{
		return SNew(STextBlock).Text(FText::FromString(Item->AssetClassPath.GetAssetName().ToString()))
			.ToolTipText(FText::FromString(Item->AssetClassPath.ToString()));
	}
	if (ColumnName == ColumnProperty)
	{
		return SNew(STextBlock).Text(FText::FromString(Item->PropertyPath));
	}
	if (ColumnName == ColumnMatched)
	{
		return SNew(STextBlock).Text(FText::FromString(Item->MatchedClassPath.GetAssetName()))
			.ToolTipText(FText::FromString(Item->MatchedClassPath.ToString()));
	}
	if (ColumnName == ColumnRefKind)
	{
		return SNew(STextBlock).Text(Item->bIsSoftReference
			? LOCTEXT("Soft", "soft")
			: LOCTEXT("Hard", "hard"));
	}
	if (ColumnName == ColumnActions)
	{
		TWeakPtr<FClassUsageMatch> WeakItem = Item;
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Open", "Open"))
				.OnClicked_Lambda([WeakItem]()
				{
					if (TSharedPtr<FClassUsageMatch> It = WeakItem.Pin())
					{
						if (UObject* Asset = It->AssetPath.TryLoad())
						{
							GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
						}
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Browse", "Browse"))
				.OnClicked_Lambda([WeakItem]()
				{
					if (TSharedPtr<FClassUsageMatch> It = WeakItem.Pin())
					{
						if (UObject* Asset = It->AssetPath.TryLoad())
						{
							FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
							TArray<UObject*> Syncs; Syncs.Add(Asset);
							CB.Get().SyncBrowserToAssets(Syncs);
						}
					}
					return FReply::Handled();
				})
			];
	}
	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
