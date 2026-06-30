#include "UEIRDataAssetJsonBPLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUEIRExampleDataAssetJsonRoundtripTest, "UEIRInterchange.DataAsset.ExampleJsonRoundtrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

static void UEIRInterchange_DeletePackageFiles(const FString& LongPackageName)
{
	const FString AssetFilename = FPackageName::LongPackageNameToFilename(LongPackageName, FPackageName::GetAssetPackageExtension());
	const FString ExpFilename = FPaths::ChangeExtension(AssetFilename, TEXT("uexp"));
	const FString BulkFilename = FPaths::ChangeExtension(AssetFilename, TEXT("ubulk"));

	IFileManager::Get().Delete(*AssetFilename, false, true, true);
	IFileManager::Get().Delete(*ExpFilename, false, true, true);
	IFileManager::Get().Delete(*BulkFilename, false, true, true);
}

bool FUEIRExampleDataAssetJsonRoundtripTest::RunTest(const FString& Parameters)
{
	const FString PackagePath = TEXT("/Game/UEIRInterchangeTests");
	const FString AssetName = TEXT("DA_JsonRoundtrip");
	const FString LongPackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
	const FString AssetFilename = FPackageName::LongPackageNameToFilename(LongPackageName, FPackageName::GetAssetPackageExtension());

	UEIRInterchange_DeletePackageFiles(LongPackageName);

	const FString JsonFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEIRInterchange"), TEXT("Roundtrip"), TEXT("ExampleDA.json"));
	IFileManager::Get().Delete(*JsonFile, false, true, true);

	UPackage* Package = CreatePackage(*LongPackageName);
	TestNotNull(TEXT("CreatePackage"), Package);

	UUEIRExampleDataAsset* Asset = NewObject<UUEIRExampleDataAsset>(Package, *AssetName, RF_Public | RF_Standalone);
	TestNotNull(TEXT("NewObject"), Asset);

	Asset->Data.Title = TEXT("RoundtripTitle");
	Asset->Data.Revision = 7;
	Asset->Data.PrimaryReference = FSoftObjectPath(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));

	FUEIRExampleEntry E0;
	E0.Key = TEXT("A");
	E0.Value = 1;
	E0.Reference = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));

	FUEIRExampleEntry E1;
	E1.Key = TEXT("B");
	E1.Value = 2;
	E1.Reference = FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Asset->Data.Entries = { E0, E1 };

	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Asset);

	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *AssetFilename, SaveArgs);
		TestTrue(TEXT("SavePackage"), bSaved);
	}

	FString ExportedJson;
	const bool bExported = UUEIRDataAssetJsonBPLibrary::ExportExampleDataAssetToJsonFile(Asset, JsonFile, ExportedJson);
	TestTrue(TEXT("ExportExampleDataAssetToJsonFile"), bExported);
	TestTrue(TEXT("ExportedJsonNotEmpty"), !ExportedJson.IsEmpty());
	TestTrue(TEXT("JsonFileExists"), IFileManager::Get().FileExists(*JsonFile));

	{
		TArray<UPackage*> PackagesToUnload;
		PackagesToUnload.Add(Package);
		UPackageTools::UnloadPackages(PackagesToUnload);
		CollectGarbage(RF_NoFlags);
	}

	UEIRInterchange_DeletePackageFiles(LongPackageName);
	TestTrue(TEXT("AssetFileDeleted"), !IFileManager::Get().FileExists(*AssetFilename));

	UUEIRExampleDataAsset* ImportedAsset = nullptr;
	FString ImportedObjectPath;
	const bool bImported = UUEIRDataAssetJsonBPLibrary::ImportExampleDataAssetFromJsonFile(JsonFile, true, ImportedAsset, ImportedObjectPath);
	TestTrue(TEXT("ImportExampleDataAssetFromJsonFile"), bImported);
	TestNotNull(TEXT("ImportedAsset"), ImportedAsset);
	TestTrue(TEXT("ImportedObjectPathNotEmpty"), !ImportedObjectPath.IsEmpty());

	if (IsValid(ImportedAsset))
	{
		TestEqual(TEXT("Title"), ImportedAsset->Data.Title, FString(TEXT("RoundtripTitle")));
		TestEqual(TEXT("Revision"), ImportedAsset->Data.Revision, 7);
		TestEqual(TEXT("PrimaryReference"), ImportedAsset->Data.PrimaryReference.ToString(), FString(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")));
		TestEqual(TEXT("EntriesNum"), ImportedAsset->Data.Entries.Num(), 2);

		if (ImportedAsset->Data.Entries.Num() == 2)
		{
			TestEqual(TEXT("Entry0Key"), ImportedAsset->Data.Entries[0].Key, FString(TEXT("A")));
			TestEqual(TEXT("Entry0Value"), ImportedAsset->Data.Entries[0].Value, 1);
			TestEqual(TEXT("Entry0Reference"), ImportedAsset->Data.Entries[0].Reference.ToString(), FString(TEXT("/Engine/BasicShapes/Cube.Cube")));

			TestEqual(TEXT("Entry1Key"), ImportedAsset->Data.Entries[1].Key, FString(TEXT("B")));
			TestEqual(TEXT("Entry1Value"), ImportedAsset->Data.Entries[1].Value, 2);
			TestEqual(TEXT("Entry1Reference"), ImportedAsset->Data.Entries[1].Reference.ToString(), FString(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		}
	}

	if (IsValid(ImportedAsset))
	{
		TArray<UPackage*> PackagesToUnload;
		PackagesToUnload.Add(ImportedAsset->GetOutermost());
		UPackageTools::UnloadPackages(PackagesToUnload);
		CollectGarbage(RF_NoFlags);
	}

	UEIRInterchange_DeletePackageFiles(LongPackageName);
	IFileManager::Get().Delete(*JsonFile, false, true, true);

	return true;
}

#endif
