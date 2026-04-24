// Copyright (c) Ruben. Licensed under the MIT License.

#include "ClassUsageFinderModule.h"
#include "SClassUsageFinderWindow.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ClassUsageFinder"

static const FName ClassUsageFinderTabId("ClassUsageFinderTab");

void FClassUsageFinderModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ClassUsageFinderTabId,
		FOnSpawnTab::CreateRaw(this, &FClassUsageFinderModule::SpawnFinderTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Class Usage Finder"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Find every asset that references a C++ class via TSubclassOf / soft class / object properties."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Class"));

	ToolMenusStartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FClassUsageFinderModule::RegisterMenus));
}

void FClassUsageFinderModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(ToolMenusStartupHandle);
	UToolMenus::UnregisterOwner(this);

	if (FGlobalTabmanager::Get()->HasTabSpawner(ClassUsageFinderTabId))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ClassUsageFinderTabId);
	}
}

void FClassUsageFinderModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Tools"))
	{
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection("Programming");
		Section.AddMenuEntry(
			"OpenClassUsageFinder",
			LOCTEXT("OpenClassUsageFinder", "Class Usage Finder"),
			LOCTEXT("OpenClassUsageFinderTip", "Find assets that reference a C++ class through reflected properties."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Class"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(ClassUsageFinderTabId);
			})));
	}
}

TSharedRef<SDockTab> FClassUsageFinderModule::SpawnFinderTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("TabTitle", "Class Usage Finder"))
		[
			SNew(SClassUsageFinderWindow)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FClassUsageFinderModule, ClassUsageFinder);
