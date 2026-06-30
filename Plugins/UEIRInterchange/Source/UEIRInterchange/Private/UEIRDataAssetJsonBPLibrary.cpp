#include "UEIRDataAssetJsonBPLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

static bool UEIRInterchange_SavePackageForAsset(UObject* Asset)
{
	if (!IsValid(Asset))
	{
		return false;
	}

	UPackage* Package = Asset->GetOutermost();
	if (!IsValid(Package))
	{
		return false;
	}

	const FString LongPackageName = Package->GetName();
	const FString Filename = FPackageName::LongPackageNameToFilename(LongPackageName, FPackageName::GetAssetPackageExtension());
	const FString Directory = FPaths::GetPath(Filename);
	if (!Directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

	return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
}

static FUEIRExampleEntryJson UEIRInterchange_ToJsonEntry(const FUEIRExampleEntry& InEntry)
{
	FUEIRExampleEntryJson Out;
	Out.Key = InEntry.Key;
	Out.Value = InEntry.Value;
	Out.Reference = InEntry.Reference.ToString();
	return Out;
}

static FUEIRExampleEntry UEIRInterchange_FromJsonEntry(const FUEIRExampleEntryJson& InEntry)
{
	FUEIRExampleEntry Out;
	Out.Key = InEntry.Key;
	Out.Value = InEntry.Value;
	Out.Reference = FSoftObjectPath(InEntry.Reference);
	return Out;
}

static FUEIRExampleDataJson UEIRInterchange_ToJsonData(const FUEIRExampleData& InData)
{
	FUEIRExampleDataJson Out;
	Out.Title = InData.Title;
	Out.Revision = InData.Revision;
	Out.PrimaryReference = InData.PrimaryReference.ToString();
	Out.Entries.Reserve(InData.Entries.Num());
	for (const FUEIRExampleEntry& Entry : InData.Entries)
	{
		Out.Entries.Add(UEIRInterchange_ToJsonEntry(Entry));
	}
	return Out;
}

static FUEIRExampleData UEIRInterchange_FromJsonData(const FUEIRExampleDataJson& InData)
{
	FUEIRExampleData Out;
	Out.Title = InData.Title;
	Out.Revision = InData.Revision;
	Out.PrimaryReference = FSoftObjectPath(InData.PrimaryReference);
	Out.Entries.Reserve(InData.Entries.Num());
	for (const FUEIRExampleEntryJson& Entry : InData.Entries)
	{
		Out.Entries.Add(UEIRInterchange_FromJsonEntry(Entry));
	}
	return Out;
}

bool UUEIRDataAssetJsonBPLibrary::ExportExampleDataAssetToJsonFile(const UUEIRExampleDataAsset* Asset, const FString& OutputFile, FString& OutJson)
{
	OutJson.Reset();
	if (!IsValid(Asset) || OutputFile.IsEmpty())
	{
		return false;
	}

	const UPackage* Package = Asset->GetOutermost();
	if (!IsValid(Package))
	{
		return false;
	}

	FUEIRExampleDataAssetPayloadJson PayloadJson;
	PayloadJson.AssetName = Asset->GetName();
	PayloadJson.PackagePath = FPackageName::GetLongPackagePath(Package->GetName());
	PayloadJson.Data = UEIRInterchange_ToJsonData(Asset->Data);

	if (!FJsonObjectConverter::UStructToJsonObjectString(PayloadJson, OutJson, 0, 0, 0, nullptr, true))
	{
		return false;
	}

	const FString FullPath = FPaths::ConvertRelativePathToFull(OutputFile);
	const FString Directory = FPaths::GetPath(FullPath);
	if (!Directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	return FFileHelper::SaveStringToFile(OutJson, *FullPath);
}

bool UUEIRDataAssetJsonBPLibrary::ImportExampleDataAssetFromJsonFile(const FString& InputFile, bool bOverwriteExisting, UUEIRExampleDataAsset*& OutAsset, FString& OutObjectPath)
{
	OutAsset = nullptr;
	OutObjectPath.Reset();

	if (InputFile.IsEmpty())
	{
		return false;
	}

	const FString FullPath = FPaths::ConvertRelativePathToFull(InputFile);
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
	{
		return false;
	}

	FUEIRExampleDataAssetPayloadJson PayloadJson;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &PayloadJson, 0, 0))
	{
		return false;
	}

	if (PayloadJson.PackagePath.IsEmpty() || PayloadJson.AssetName.IsEmpty())
	{
		return false;
	}

	const FString LongPackageName = FString::Printf(TEXT("%s/%s"), *PayloadJson.PackagePath, *PayloadJson.AssetName);
	UUEIRExampleDataAsset* Asset = nullptr;

	if (FPackageName::DoesPackageExist(LongPackageName))
	{
		UPackage* LoadedPackage = LoadPackage(nullptr, *LongPackageName, LOAD_None);
		if (IsValid(LoadedPackage))
		{
			Asset = FindObject<UUEIRExampleDataAsset>(LoadedPackage, *PayloadJson.AssetName);
		}
	}

	const bool bExisting = IsValid(Asset);
	if (bExisting && !bOverwriteExisting)
	{
		return false;
	}

	if (!bExisting)
	{
		UPackage* NewPackage = CreatePackage(*LongPackageName);
		if (!IsValid(NewPackage))
		{
			return false;
		}

		Asset = NewObject<UUEIRExampleDataAsset>(NewPackage, *PayloadJson.AssetName, RF_Public | RF_Standalone);
		if (!IsValid(Asset))
		{
			return false;
		}

		FAssetRegistryModule::AssetCreated(Asset);
	}

	Asset->Modify();
	Asset->Data = UEIRInterchange_FromJsonData(PayloadJson.Data);
	Asset->MarkPackageDirty();

	if (!UEIRInterchange_SavePackageForAsset(Asset))
	{
		return false;
	}

	OutAsset = Asset;
	OutObjectPath = Asset->GetPathName();
	return true;
}
