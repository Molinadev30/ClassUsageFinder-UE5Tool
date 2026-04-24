// Copyright (c) Ruben. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "Delegates/Delegate.h"

class UClass;
class UObject;

struct CLASSUSAGEFINDER_API FClassUsageMatch
{
	FSoftObjectPath AssetPath;
	FTopLevelAssetPath AssetClassPath;
	FString PropertyPath;
	FSoftObjectPath MatchedClassPath;
	bool bIsSoftReference = false;
};

struct CLASSUSAGEFINDER_API FClassUsageScanOptions
{
	TWeakObjectPtr<UClass> TargetClass;
	bool bIncludeSubclasses = true;
	bool bScanDataAssets = true;
	bool bScanBlueprints = true;
	bool bScanAllUObjectAssets = false;
	bool bResolveSoftRefs = true;
};

DECLARE_DELEGATE_TwoParams(FOnClassUsageScanProgress, int32 /*Current*/, int32 /*Total*/);

class CLASSUSAGEFINDER_API FClassUsageScanner
{
public:
	static TArray<FClassUsageMatch> ScanProject(
		const FClassUsageScanOptions& Options,
		FOnClassUsageScanProgress OnProgress = FOnClassUsageScanProgress());
};
