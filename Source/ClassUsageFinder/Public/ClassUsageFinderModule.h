// Copyright (c) Ruben. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FSpawnTabArgs;
class SDockTab;

class FClassUsageFinderModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	TSharedRef<SDockTab> SpawnFinderTab(const FSpawnTabArgs& Args);

	FDelegateHandle ToolMenusStartupHandle;
};
