// Copyright (c) Ruben. Licensed under the MIT License.

#include "ClassUsageScanner.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPtr.h"

#define LOCTEXT_NAMESPACE "ClassUsageFinder"

namespace ClassUsageFinderPrivate
{
	static bool ClassMatchesTarget(const UClass* Candidate, const UClass* Target, bool bIncludeSubclasses)
	{
		if (!Candidate || !Target) return false;
		return bIncludeSubclasses ? Candidate->IsChildOf(Target) : Candidate == Target;
	}

	static void RecordMatch(
		UObject* OwnerAsset,
		const UClass* MatchedClass,
		const FString& PropertyPath,
		bool bSoft,
		TArray<FClassUsageMatch>& OutMatches)
	{
		if (!OwnerAsset || !MatchedClass) return;

		FClassUsageMatch M;
		M.AssetPath = FSoftObjectPath(OwnerAsset);
		M.AssetClassPath = OwnerAsset->GetClass()->GetClassPathName();
		M.PropertyPath = PropertyPath;
		M.MatchedClassPath = FSoftObjectPath(MatchedClass);
		M.bIsSoftReference = bSoft;
		OutMatches.Add(MoveTemp(M));
	}

	static void WalkProperty(
		FProperty* Property,
		const void* ValuePtr,
		const FString& PathSoFar,
		UObject* OwnerAsset,
		const FClassUsageScanOptions& Options,
		TArray<FClassUsageMatch>& OutMatches,
		TSet<const void*>& VisitedStructs,
		TSet<const UObject*>& VisitedObjects);

	static void WalkStruct(
		const UStruct* Struct,
		const void* StructPtr,
		const FString& PathSoFar,
		UObject* OwnerAsset,
		const FClassUsageScanOptions& Options,
		TArray<FClassUsageMatch>& OutMatches,
		TSet<const void*>& VisitedStructs,
		TSet<const UObject*>& VisitedObjects)
	{
		if (!Struct || !StructPtr) return;
		if (VisitedStructs.Contains(StructPtr)) return;
		VisitedStructs.Add(StructPtr);

		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			const FString NextPath = PathSoFar.IsEmpty()
				? Prop->GetName()
				: (PathSoFar + TEXT(".") + Prop->GetName());
			const void* Value = Prop->ContainerPtrToValuePtr<void>(StructPtr);
			WalkProperty(Prop, Value, NextPath, OwnerAsset, Options, OutMatches, VisitedStructs, VisitedObjects);
		}
	}

	static void WalkProperty(
		FProperty* Property,
		const void* ValuePtr,
		const FString& PathSoFar,
		UObject* OwnerAsset,
		const FClassUsageScanOptions& Options,
		TArray<FClassUsageMatch>& OutMatches,
		TSet<const void*>& VisitedStructs,
		TSet<const UObject*>& VisitedObjects)
	{
		if (!Property || !ValuePtr) return;
		UClass* const TargetClass = Options.TargetClass.Get();
		if (!TargetClass) return;

		// Hard class refs: TSubclassOf<T>
		if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
		{
			UObject* Obj = ClassProp->GetObjectPropertyValue(ValuePtr);
			if (UClass* AsClass = Cast<UClass>(Obj))
			{
				if (ClassMatchesTarget(AsClass, TargetClass, Options.bIncludeSubclasses))
				{
					RecordMatch(OwnerAsset, AsClass, PathSoFar, /*bSoft*/ false, OutMatches);
				}
			}
			return;
		}

		// Soft class refs: TSoftClassPtr<T>
		if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
		{
			const FSoftObjectPtr& Ptr = *reinterpret_cast<const FSoftObjectPtr*>(ValuePtr);
			const FSoftObjectPath SoftPath = Ptr.ToSoftObjectPath();
			if (SoftPath.IsNull()) return;

			UClass* Resolved = Cast<UClass>(Ptr.Get());
			if (!Resolved && Options.bResolveSoftRefs)
			{
				Resolved = Cast<UClass>(SoftPath.TryLoad());
			}
			if (ClassMatchesTarget(Resolved, TargetClass, Options.bIncludeSubclasses))
			{
				RecordMatch(OwnerAsset, Resolved, PathSoFar, /*bSoft*/ true, OutMatches);
			}
			return;
		}

		// Generic soft object ref (rare for classes, but supported)
		if (const FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
		{
			if (SoftObjProp->PropertyClass && SoftObjProp->PropertyClass->IsChildOf(UClass::StaticClass()))
			{
				const FSoftObjectPtr& Ptr = *reinterpret_cast<const FSoftObjectPtr*>(ValuePtr);
				const FSoftObjectPath SoftPath = Ptr.ToSoftObjectPath();
				if (SoftPath.IsNull()) return;

				UClass* Resolved = Cast<UClass>(Ptr.Get());
				if (!Resolved && Options.bResolveSoftRefs)
				{
					Resolved = Cast<UClass>(SoftPath.TryLoad());
				}
				if (ClassMatchesTarget(Resolved, TargetClass, Options.bIncludeSubclasses))
				{
					RecordMatch(OwnerAsset, Resolved, PathSoFar, /*bSoft*/ true, OutMatches);
				}
			}
			return;
		}

		// Object property: either a class pointer (rare — usually hidden behind FClassProperty)
		// or an instanced sub-object we should walk into.
		if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
			if (!Obj) return;

			if (UClass* AsClass = Cast<UClass>(Obj))
			{
				if (ClassMatchesTarget(AsClass, TargetClass, Options.bIncludeSubclasses))
				{
					RecordMatch(OwnerAsset, AsClass, PathSoFar, /*bSoft*/ false, OutMatches);
				}
				return;
			}

			// Don't cross into unrelated external assets; those are separate nodes in the graph.
			if (Obj->IsAsset() && Obj != OwnerAsset) return;
			if (VisitedObjects.Contains(Obj)) return;
			VisitedObjects.Add(Obj);

			WalkStruct(Obj->GetClass(), Obj, PathSoFar, OwnerAsset, Options, OutMatches, VisitedStructs, VisitedObjects);
			return;
		}

		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProp, ValuePtr);
			for (int32 i = 0; i < Helper.Num(); ++i)
			{
				const FString ElemPath = FString::Printf(TEXT("%s[%d]"), *PathSoFar, i);
				WalkProperty(ArrayProp->Inner, Helper.GetRawPtr(i), ElemPath, OwnerAsset, Options, OutMatches, VisitedStructs, VisitedObjects);
			}
			return;
		}

		if (const FSetProperty* SetProp = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper Helper(SetProp, ValuePtr);
			for (int32 i = 0; i < Helper.GetMaxIndex(); ++i)
			{
				if (!Helper.IsValidIndex(i)) continue;
				const FString ElemPath = FString::Printf(TEXT("%s{%d}"), *PathSoFar, i);
				WalkProperty(SetProp->ElementProp, Helper.GetElementPtr(i), ElemPath, OwnerAsset, Options, OutMatches, VisitedStructs, VisitedObjects);
			}
			return;
		}

		if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper Helper(MapProp, ValuePtr);
			for (int32 i = 0; i < Helper.GetMaxIndex(); ++i)
			{
				if (!Helper.IsValidIndex(i)) continue;
				const FString KeyPath = FString::Printf(TEXT("%s<key %d>"), *PathSoFar, i);
				const FString ValPath = FString::Printf(TEXT("%s<val %d>"), *PathSoFar, i);
				WalkProperty(MapProp->KeyProp, Helper.GetKeyPtr(i), KeyPath, OwnerAsset, Options, OutMatches, VisitedStructs, VisitedObjects);
				WalkProperty(MapProp->ValueProp, Helper.GetValuePtr(i), ValPath, OwnerAsset, Options, OutMatches, VisitedStructs, VisitedObjects);
			}
			return;
		}

		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			WalkStruct(StructProp->Struct, ValuePtr, PathSoFar, OwnerAsset, Options, OutMatches, VisitedStructs, VisitedObjects);
			return;
		}
	}
}

TArray<FClassUsageMatch> FClassUsageScanner::ScanProject(
	const FClassUsageScanOptions& Options,
	FOnClassUsageScanProgress OnProgress)
{
	using namespace ClassUsageFinderPrivate;

	TArray<FClassUsageMatch> Matches;
	UClass* const TargetClass = Options.TargetClass.Get();
	if (!TargetClass) return Matches;

	FAssetRegistryModule& ARModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AR = ARModule.Get();
	AR.WaitForCompletion();

	TArray<FAssetData> Assets;
	if (Options.bScanAllUObjectAssets)
	{
		AR.GetAllAssets(Assets, /*bIncludeOnlyOnDiskAssets=*/ false);
	}
	else
	{
		FARFilter Filter;
		Filter.bRecursiveClasses = true;
		if (Options.bScanDataAssets)
		{
			Filter.ClassPaths.Add(UDataAsset::StaticClass()->GetClassPathName());
		}
		if (Options.bScanBlueprints)
		{
			Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		}
		if (Filter.ClassPaths.Num() == 0) return Matches;
		AR.GetAssets(Filter, Assets);
	}

	const int32 Total = Assets.Num();
	FScopedSlowTask SlowTask(static_cast<float>(Total),
		LOCTEXT("ScanningProject", "Scanning project for class references..."));
	SlowTask.MakeDialog(/*bShowCancelButton=*/ true);

	for (int32 Idx = 0; Idx < Total; ++Idx)
	{
		if (SlowTask.ShouldCancel()) break;

		const FAssetData& Data = Assets[Idx];
		SlowTask.EnterProgressFrame(1.0f, FText::FromName(Data.AssetName));
		(void)OnProgress.ExecuteIfBound(Idx, Total);

		UObject* Asset = Data.GetAsset();
		if (!Asset) continue;

		TSet<const void*> VisitedStructs;
		TSet<const UObject*> VisitedObjects;

		if (UBlueprint* BP = Cast<UBlueprint>(Asset))
		{
			if (BP->GeneratedClass)
			{
				if (UObject* CDO = BP->GeneratedClass->GetDefaultObject(true))
				{
					VisitedObjects.Add(CDO);
					WalkStruct(CDO->GetClass(), CDO, FString(), BP, Options, Matches, VisitedStructs, VisitedObjects);
				}
			}
			continue;
		}

		VisitedObjects.Add(Asset);
		WalkStruct(Asset->GetClass(), Asset, FString(), Asset, Options, Matches, VisitedStructs, VisitedObjects);
	}

	return Matches;
}

#undef LOCTEXT_NAMESPACE
