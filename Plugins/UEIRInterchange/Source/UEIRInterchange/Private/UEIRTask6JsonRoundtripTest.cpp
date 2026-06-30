#include "UEIRTask6JsonBPLibrary.h"

#include "UEIRTask6JsonTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

#if UEIR_WITH_NIAGARA
#include "NiagaraParameterStore.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

static void UEIRTask6_DeletePackageFiles(const FString& LongPackageName)
{
	const FString AssetFilename = FPackageName::LongPackageNameToFilename(LongPackageName, FPackageName::GetAssetPackageExtension());
	const FString ExpFilename = FPaths::ChangeExtension(AssetFilename, TEXT("uexp"));
	const FString BulkFilename = FPaths::ChangeExtension(AssetFilename, TEXT("ubulk"));

	IFileManager::Get().Delete(*AssetFilename, false, true, true);
	IFileManager::Get().Delete(*ExpFilename, false, true, true);
	IFileManager::Get().Delete(*BulkFilename, false, true, true);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUEIRTask6_MaterialInstanceConstantJsonRoundtripTest, "UEIRInterchange.Task6.MaterialInstanceConstant.JsonRoundtrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEIRTask6_MaterialInstanceConstantJsonRoundtripTest::RunTest(const FString& Parameters)
{
	const FString PackagePath = TEXT("/Game/UEIRInterchangeTests");
	const FString AssetName = TEXT("MI_Task6JsonRoundtrip");
	const FString LongPackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
	const FString AssetFilename = FPackageName::LongPackageNameToFilename(LongPackageName, FPackageName::GetAssetPackageExtension());

	UEIRTask6_DeletePackageFiles(LongPackageName);

	const FString JsonFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEIRInterchange"), TEXT("Roundtrip"), TEXT("Task6_MaterialInstance.json"));
	IFileManager::Get().Delete(*JsonFile, false, true, true);

	UPackage* Package = CreatePackage(*LongPackageName);
	TestNotNull(TEXT("CreatePackage"), Package);

	UMaterialInstanceConstant* Asset = NewObject<UMaterialInstanceConstant>(Package, *AssetName, RF_Public | RF_Standalone);
	TestNotNull(TEXT("NewObject"), Asset);

	if (IsValid(Asset))
	{
		//Asset->SetParentEditorOnly(UMaterial::GetDefaultMaterial(MD_Surface));

		FScalarParameterValue S;
		S.ParameterInfo = FMaterialParameterInfo(FName(TEXT("TestScalar")));
		S.ParameterValue = 0.75f;
		Asset->ScalarParameterValues.Add(S);

		FVectorParameterValue V;
		V.ParameterInfo = FMaterialParameterInfo(FName(TEXT("TestVector")));
		V.ParameterValue = FLinearColor(1.0f, 0.5f, 0.25f, 1.0f);
		Asset->VectorParameterValues.Add(V);

		FTextureParameterValue T;
		T.ParameterInfo = FMaterialParameterInfo(FName(TEXT("TestTexture")));
		T.ParameterValue = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		Asset->TextureParameterValues.Add(T);

		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *AssetFilename, SaveArgs);
		TestTrue(TEXT("SavePackage"), bSaved);
	}

	FString ExportedJson;
	const bool bExported = UUEIRTask6JsonBPLibrary::ExportMaterialInstanceConstantToJsonFile(Asset, JsonFile, ExportedJson);
	TestTrue(TEXT("ExportMaterialInstanceConstantToJsonFile"), bExported);
	TestTrue(TEXT("ExportedJsonNotEmpty"), !ExportedJson.IsEmpty());
	TestTrue(TEXT("JsonFileExists"), IFileManager::Get().FileExists(*JsonFile));

	{
		FUEIRMaterialInstanceConstantPayloadJson Payload;
		const bool bParsed = FJsonObjectConverter::JsonObjectStringToUStruct(ExportedJson, &Payload, 0, 0);
		TestTrue(TEXT("JsonParsePayload"), bParsed);
		if (bParsed)
		{
			TestEqual(TEXT("PayloadAssetName"), Payload.AssetName, AssetName);
			TestTrue(TEXT("PayloadParentNotEmpty"), !Payload.ParentObjectPath.IsEmpty());
			TestTrue(TEXT("ScalarCount"), Payload.ScalarParameters.Num() == 1);
			TestTrue(TEXT("VectorCount"), Payload.VectorParameters.Num() == 1);
			TestTrue(TEXT("TextureCount"), Payload.TextureParameters.Num() == 1);
		}
	}

	{
		TArray<UPackage*> PackagesToUnload;
		PackagesToUnload.Add(Package);
		UPackageTools::UnloadPackages(PackagesToUnload);
		CollectGarbage(RF_NoFlags);
	}

	UEIRTask6_DeletePackageFiles(LongPackageName);
	TestTrue(TEXT("AssetFileDeleted"), !IFileManager::Get().FileExists(*AssetFilename));

	UMaterialInstanceConstant* Imported = nullptr;
	FString ImportedPath;
	FString ImportReport;
	const bool bImported = UUEIRTask6JsonBPLibrary::ImportMaterialInstanceConstantFromJsonFile(JsonFile, true, Imported, ImportedPath, ImportReport);
	TestTrue(TEXT("ImportMaterialInstanceConstantFromJsonFile"), bImported);
	TestNotNull(TEXT("ImportedAsset"), Imported);
	TestTrue(TEXT("ImportedPathNotEmpty"), !ImportedPath.IsEmpty());

	if (IsValid(Imported))
	{
		TestTrue(TEXT("ImportedParentNotNull"), IsValid(Imported->Parent));
		TestTrue(TEXT("ImportedScalarCount"), Imported->ScalarParameterValues.Num() == 1);
		TestTrue(TEXT("ImportedVectorCount"), Imported->VectorParameterValues.Num() == 1);
		TestTrue(TEXT("ImportedTextureCount"), Imported->TextureParameterValues.Num() == 1);
	}

	if (IsValid(Imported))
	{
		TArray<UPackage*> PackagesToUnload;
		PackagesToUnload.Add(Imported->GetOutermost());
		UPackageTools::UnloadPackages(PackagesToUnload);
		CollectGarbage(RF_NoFlags);
	}

	UEIRTask6_DeletePackageFiles(LongPackageName);
	IFileManager::Get().Delete(*JsonFile, false, true, true);

	return true;
}

#if UEIR_WITH_NIAGARA

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUEIRTask6_NiagaraUserParamsJsonExportImportTest, "UEIRInterchange.Task6.Niagara.UserParams.ExportImport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEIRTask6_NiagaraUserParamsJsonExportImportTest::RunTest(const FString& Parameters)
{
	const FString PackagePath = TEXT("/Game/UEIRInterchangeTests");
	const FString AssetName = TEXT("NS_Task6UserParams");
	const FString LongPackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
	const FString AssetFilename = FPackageName::LongPackageNameToFilename(LongPackageName, FPackageName::GetAssetPackageExtension());

	UEIRTask6_DeletePackageFiles(LongPackageName);

	const FString JsonFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEIRInterchange"), TEXT("Roundtrip"), TEXT("Task6_NiagaraUserParams.json"));
	IFileManager::Get().Delete(*JsonFile, false, true, true);

	UPackage* Package = CreatePackage(*LongPackageName);
	TestNotNull(TEXT("CreatePackage"), Package);

	UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, *AssetName, RF_Public | RF_Standalone);
	TestNotNull(TEXT("NewNiagaraSystem"), System);

	if (IsValid(System))
	{
		FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();

		FNiagaraVariable FloatVar(FNiagaraTypeDefinition::GetFloatDef(), FName(TEXT("User.TestFloat")));
		Store.AddParameter(FloatVar, true);
		Store.SetParameterValue(42.0f, FloatVar);

		FNiagaraVariable ColorVar(FNiagaraTypeDefinition::GetColorDef(), FName(TEXT("User.TestColor")));
		Store.AddParameter(ColorVar, true);
		Store.SetParameterValue(FLinearColor(0.25f, 0.5f, 0.75f, 1.0f), ColorVar);

		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(System);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, System, *AssetFilename, SaveArgs);
		TestTrue(TEXT("SavePackage"), bSaved);
	}

	FString ExportedJson;
	FString ExportReport;
	const bool bExported = UUEIRTask6JsonBPLibrary::ExportNiagaraSystemUserParametersToJsonFile(System->GetPathName(), JsonFile, ExportedJson, ExportReport);
	TestTrue(TEXT("ExportNiagaraSystemUserParametersToJsonFile"), bExported);
	TestTrue(TEXT("ExportedJsonNotEmpty"), !ExportedJson.IsEmpty());
	TestTrue(TEXT("JsonFileExists"), IFileManager::Get().FileExists(*JsonFile));

	FUEIRNiagaraSystemUserParametersPayloadJson Payload;
	const bool bParsed = FJsonObjectConverter::JsonObjectStringToUStruct(ExportedJson, &Payload, 0, 0);
	TestTrue(TEXT("JsonParsePayload"), bParsed);
	if (bParsed)
	{
		TestEqual(TEXT("PayloadSystemPath"), Payload.SystemObjectPath, System->GetPathName());
		TestTrue(TEXT("PayloadParamCount"), Payload.UserParameters.Num() >= 2);
	}

	FUEIRNiagaraSystemUserParametersPayloadJson NewPayload;
	NewPayload.SystemObjectPath = System->GetPathName();
	{
		FUEIRNiagaraUserParameterJson P;
		P.Name = TEXT("User.TestFloat");
		P.Type = TEXT("Float");
		P.NumberValue = 100.0;
		NewPayload.UserParameters.Add(P);
	}
	{
	}

	FString NewJson;
	const bool bNewJson = FJsonObjectConverter::UStructToJsonObjectString(NewPayload, NewJson, 0, 0, 0, nullptr, true);
	TestTrue(TEXT("NewPayloadToJson"), bNewJson);
	if (bNewJson)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(JsonFile), true);
		FFileHelper::SaveStringToFile(NewJson, *JsonFile);
	}

	FString ImportReport;
	const bool bImported = UUEIRTask6JsonBPLibrary::ImportNiagaraSystemUserParametersFromJsonFile(JsonFile, System->GetPathName(), true, true, ImportReport);
	TestTrue(TEXT("ImportNiagaraSystemUserParametersFromJsonFile"), bImported);

	if (IsValid(System))
	{
		const FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
		const FNiagaraVariable FloatVar(FNiagaraTypeDefinition::GetFloatDef(), FName(TEXT("User.TestFloat")));
		TestEqual(TEXT("AppliedFloat"), Store.GetParameterValue<float>(FloatVar), 100.0f);
	}

	{
		TArray<UPackage*> PackagesToUnload;
		PackagesToUnload.Add(Package);
		UPackageTools::UnloadPackages(PackagesToUnload);
		CollectGarbage(RF_NoFlags);
	}

	UEIRTask6_DeletePackageFiles(LongPackageName);
	IFileManager::Get().Delete(*JsonFile, false, true, true);

	return true;
}

#else

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUEIRTask6_NiagaraUserParamsJsonNotEnabledTest, "UEIRInterchange.Task6.Niagara.UserParams.NotEnabled", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEIRTask6_NiagaraUserParamsJsonNotEnabledTest::RunTest(const FString& Parameters)
{
	const FString JsonFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEIRInterchange"), TEXT("Roundtrip"), TEXT("Task6_NiagaraUserParams_NotEnabled.json"));
	IFileManager::Get().Delete(*JsonFile, false, true, true);

	FUEIRNiagaraSystemUserParametersPayloadJson Payload;
	Payload.SystemObjectPath = TEXT("/Game/DoesNotMatter.NS_DoesNotMatter");
	{
		FUEIRNiagaraUserParameterJson P;
		P.Name = TEXT("User.TestFloat");
		P.Type = TEXT("Float");
		P.NumberValue = 1.0;
		Payload.UserParameters.Add(P);
	}

	FString Json;
	const bool bToJson = FJsonObjectConverter::UStructToJsonObjectString(Payload, Json, 0, 0, 0, nullptr, true);
	TestTrue(TEXT("PayloadToJson"), bToJson);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(JsonFile), true);
	FFileHelper::SaveStringToFile(Json, *JsonFile);

	FString Report;
	const bool bOk = UUEIRTask6JsonBPLibrary::ImportNiagaraSystemUserParametersFromJsonFile(JsonFile, TEXT("/Game/Nope.Nope"), true, true, Report);
	TestTrue(TEXT("ImportShouldFailWhenNiagaraDisabled"), !bOk);
	TestTrue(TEXT("ReportMentionsNotEnabled"), Report.Contains(TEXT("Niagara未启用")) || Report.Contains(TEXT("UEIR_WITH_NIAGARA: 0")));

	IFileManager::Get().Delete(*JsonFile, false, true, true);
	return true;
}

#endif

#endif
