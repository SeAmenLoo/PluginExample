#include "UEIRTask6JsonBPLibrary.h"

#include "UEIRTask6JsonTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

#if UEIR_WITH_NIAGARA
#include "NiagaraParameterStore.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#endif

static bool UEIRTask6_SavePackageForAsset(UObject* Asset)
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

static bool UEIRTask6_SaveStringToFileEnsureDir(const FString& OutputFile, const FString& Text)
{
	if (OutputFile.IsEmpty())
	{
		return false;
	}

	const FString FullPath = FPaths::ConvertRelativePathToFull(OutputFile);
	const FString Directory = FPaths::GetPath(FullPath);
	if (!Directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}
	return FFileHelper::SaveStringToFile(Text, *FullPath);
}

static bool UEIRTask6_LoadFileToString(const FString& InputFile, FString& OutText)
{
	OutText.Reset();
	if (InputFile.IsEmpty())
	{
		return false;
	}
	const FString FullPath = FPaths::ConvertRelativePathToFull(InputFile);
	return FFileHelper::LoadFileToString(OutText, *FullPath);
}

static FUEIRMaterialInstanceConstantPayloadJson UEIRTask6_ToJsonPayload(const UMaterialInstanceConstant* Asset)
{
	FUEIRMaterialInstanceConstantPayloadJson Payload;
	if (!IsValid(Asset))
	{
		return Payload;
	}

	const UPackage* Package = Asset->GetOutermost();
	Payload.AssetName = Asset->GetName();
	Payload.PackagePath = IsValid(Package) ? FPackageName::GetLongPackagePath(Package->GetName()) : FString();
	Payload.ParentObjectPath = IsValid(Asset->Parent) ? Asset->Parent->GetPathName() : FString();

	Payload.ScalarParameters.Reserve(Asset->ScalarParameterValues.Num());
	for (const FScalarParameterValue& P : Asset->ScalarParameterValues)
	{
		FUEIRMaterialScalarParameterJson J;
		J.Name = P.ParameterInfo.Name.ToString();
		J.Value = P.ParameterValue;
		Payload.ScalarParameters.Add(MoveTemp(J));
	}

	Payload.VectorParameters.Reserve(Asset->VectorParameterValues.Num());
	for (const FVectorParameterValue& P : Asset->VectorParameterValues)
	{
		FUEIRMaterialVectorParameterJson J;
		J.Name = P.ParameterInfo.Name.ToString();
		J.Value = P.ParameterValue;
		Payload.VectorParameters.Add(MoveTemp(J));
	}

	Payload.TextureParameters.Reserve(Asset->TextureParameterValues.Num());
	for (const FTextureParameterValue& P : Asset->TextureParameterValues)
	{
		FUEIRMaterialTextureParameterJson J;
		J.Name = P.ParameterInfo.Name.ToString();
		J.TextureObjectPath = IsValid(P.ParameterValue) ? P.ParameterValue->GetPathName() : FString();
		Payload.TextureParameters.Add(MoveTemp(J));
	}

	return Payload;
}

static bool UEIRTask6_ApplyMaterialInstancePayload(UMaterialInstanceConstant* Asset, const FUEIRMaterialInstanceConstantPayloadJson& Payload, FString& OutReport)
{
	OutReport.Reset();
	if (!IsValid(Asset))
	{
		OutReport = TEXT("Asset无效");
		return false;
	}

	UMaterialInterface* Parent = nullptr;
	if (!Payload.ParentObjectPath.IsEmpty())
	{
		Parent = LoadObject<UMaterialInterface>(nullptr, *Payload.ParentObjectPath);
		if (!IsValid(Parent))
		{
			OutReport += FString::Printf(TEXT("- Parent无法加载: %s\n"), *Payload.ParentObjectPath);
		}
	}

	Asset->Modify();

	if (IsValid(Parent))
	{
		Asset->SetParentEditorOnly(Parent);
	}

	Asset->ScalarParameterValues.Reset();
	Asset->VectorParameterValues.Reset();
	Asset->TextureParameterValues.Reset();

	for (const FUEIRMaterialScalarParameterJson& J : Payload.ScalarParameters)
	{
		if (J.Name.IsEmpty())
		{
			continue;
		}
		FScalarParameterValue P;
		P.ParameterInfo = FMaterialParameterInfo(FName(*J.Name));
		P.ParameterValue = J.Value;
		Asset->ScalarParameterValues.Add(MoveTemp(P));
	}

	for (const FUEIRMaterialVectorParameterJson& J : Payload.VectorParameters)
	{
		if (J.Name.IsEmpty())
		{
			continue;
		}
		FVectorParameterValue P;
		P.ParameterInfo = FMaterialParameterInfo(FName(*J.Name));
		P.ParameterValue = J.Value;
		Asset->VectorParameterValues.Add(MoveTemp(P));
	}

	for (const FUEIRMaterialTextureParameterJson& J : Payload.TextureParameters)
	{
		if (J.Name.IsEmpty())
		{
			continue;
		}
		FTextureParameterValue P;
		P.ParameterInfo = FMaterialParameterInfo(FName(*J.Name));
		P.ParameterValue = J.TextureObjectPath.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *J.TextureObjectPath);
		if (!J.TextureObjectPath.IsEmpty() && !IsValid(P.ParameterValue))
		{
			OutReport += FString::Printf(TEXT("- Texture无法加载: %s (Param=%s)\n"), *J.TextureObjectPath, *J.Name);
		}
		Asset->TextureParameterValues.Add(MoveTemp(P));
	}

	Asset->MarkPackageDirty();
	Asset->PostEditChange();
	return true;
}

bool UUEIRTask6JsonBPLibrary::ExportMaterialInstanceConstantToJsonFile(const UMaterialInstanceConstant* Asset, const FString& OutputFile, FString& OutJson)
{
	OutJson.Reset();
	if (!IsValid(Asset) || OutputFile.IsEmpty())
	{
		return false;
	}

	const FUEIRMaterialInstanceConstantPayloadJson Payload = UEIRTask6_ToJsonPayload(Asset);
	if (!FJsonObjectConverter::UStructToJsonObjectString(Payload, OutJson, 0, 0, 0, nullptr, true))
	{
		return false;
	}

	return UEIRTask6_SaveStringToFileEnsureDir(OutputFile, OutJson);
}

bool UUEIRTask6JsonBPLibrary::ImportMaterialInstanceConstantFromJsonFile(const FString& InputFile, bool bOverwriteExisting, UMaterialInstanceConstant*& OutAsset, FString& OutObjectPath, FString& OutReportText)
{
	OutAsset = nullptr;
	OutObjectPath.Reset();
	OutReportText.Reset();

	FString JsonString;
	if (!UEIRTask6_LoadFileToString(InputFile, JsonString))
	{
		OutReportText = TEXT("读取JSON失败");
		return false;
	}

	FUEIRMaterialInstanceConstantPayloadJson Payload;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &Payload, 0, 0))
	{
		OutReportText = TEXT("解析JSON失败");
		return false;
	}

	if (Payload.PackagePath.IsEmpty() || Payload.AssetName.IsEmpty())
	{
		OutReportText = TEXT("JSON缺少PackagePath或AssetName");
		return false;
	}

	const FString LongPackageName = FString::Printf(TEXT("%s/%s"), *Payload.PackagePath, *Payload.AssetName);
	UMaterialInstanceConstant* Asset = nullptr;

	if (FPackageName::DoesPackageExist(LongPackageName))
	{
		UPackage* LoadedPackage = LoadPackage(nullptr, *LongPackageName, LOAD_None);
		if (IsValid(LoadedPackage))
		{
			Asset = FindObject<UMaterialInstanceConstant>(LoadedPackage, *Payload.AssetName);
		}
	}

	const bool bExisting = IsValid(Asset);
	if (bExisting && !bOverwriteExisting)
	{
		OutReportText = TEXT("目标资产已存在且未允许覆盖");
		return false;
	}

	if (!bExisting)
	{
		UPackage* NewPackage = CreatePackage(*LongPackageName);
		if (!IsValid(NewPackage))
		{
			OutReportText = TEXT("CreatePackage失败");
			return false;
		}

		Asset = NewObject<UMaterialInstanceConstant>(NewPackage, *Payload.AssetName, RF_Public | RF_Standalone);
		if (!IsValid(Asset))
		{
			OutReportText = TEXT("创建UMaterialInstanceConstant失败");
			return false;
		}

		FAssetRegistryModule::AssetCreated(Asset);
	}

	FString ApplyReport;
	if (!UEIRTask6_ApplyMaterialInstancePayload(Asset, Payload, ApplyReport))
	{
		OutReportText = ApplyReport;
		return false;
	}
	OutReportText = ApplyReport;

	if (!UEIRTask6_SavePackageForAsset(Asset))
	{
		OutReportText += TEXT("保存Package失败\n");
		return false;
	}

	OutAsset = Asset;
	OutObjectPath = Asset->GetPathName();
	return true;
}

static FString UEIRTask6_BuildNiagaraReportHeader(const FString& Title)
{
	FString Out;
	Out += FString::Printf(TEXT("# %s\n\n"), *Title);
	Out += FString::Printf(TEXT("- UEIR_WITH_NIAGARA: %s\n"), UEIR_WITH_NIAGARA ? TEXT("1") : TEXT("0"));
	Out += TEXT("\n");
	return Out;
}

#if UEIR_WITH_NIAGARA

static FString UEIRTask6_NiagaraTypeToString(const FNiagaraTypeDefinition& TypeDef)
{
	if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
	{
		return TEXT("Float");
	}
	if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
	{
		return TEXT("Int");
	}
	if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
	{
		return TEXT("Bool");
	}
	if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
	{
		return TEXT("Vector2");
	}
	if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
	{
		return TEXT("Vector");
	}
	if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
	{
		return TEXT("Vector4");
	}
	if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
	{
		return TEXT("Color");
	}
	return TypeDef.GetName();
}

static bool UEIRTask6_NiagaraTryReadParam(const FNiagaraUserRedirectionParameterStore& Store, const FNiagaraVariable& Var, FUEIRNiagaraUserParameterJson& OutJson, FString& OutWarn)
{
	OutWarn.Reset();
	OutJson = FUEIRNiagaraUserParameterJson();
	OutJson.Name = Var.GetName().ToString();
	OutJson.Type = UEIRTask6_NiagaraTypeToString(Var.GetType());

	const FNiagaraTypeDefinition& TypeDef = Var.GetType();
	if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
	{
		OutJson.NumberValue = Store.GetParameterValue<float>(Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
	{
		OutJson.IntValue = Store.GetParameterValue<int32>(Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
	{
		OutJson.BoolValue = Store.GetParameterValue<bool>(Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
	{
		OutJson.Vector2Value = Store.GetParameterValue<FVector2f>(Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
	{
		OutJson.Vector3Value = Store.GetParameterValue<FVector3f>(Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
	{
		OutJson.Vector4Value = Store.GetParameterValue<FVector4f>(Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
	{
		OutJson.ColorValue = Store.GetParameterValue<FLinearColor>(Var);
		return true;
	}

	OutWarn = FString::Printf(TEXT("不支持导出类型: %s (%s)"), *OutJson.Name, *OutJson.Type);
	return false;
}

static bool UEIRTask6_NiagaraTrySetParam(FNiagaraUserRedirectionParameterStore& Store, const FUEIRNiagaraUserParameterJson& InJson, bool bAddMissing, FString& OutWarn)
{
	OutWarn.Reset();
	if (InJson.Name.IsEmpty() || InJson.Type.IsEmpty())
	{
		OutWarn = TEXT("参数Name/Type为空");
		return false;
	}

	FString FullName = InJson.Name;
	if (!FullName.StartsWith(TEXT("User."), ESearchCase::IgnoreCase))
	{
		FullName = TEXT("User.") + FullName;
	}

	FNiagaraTypeDefinition TypeDef;
	const FString TypeLower = InJson.Type.ToLower();
	if (TypeLower == TEXT("float"))
	{
		TypeDef = FNiagaraTypeDefinition::GetFloatDef();
	}
	else if (TypeLower == TEXT("int") || TypeLower == TEXT("int32"))
	{
		TypeDef = FNiagaraTypeDefinition::GetIntDef();
	}
	else if (TypeLower == TEXT("bool"))
	{
		TypeDef = FNiagaraTypeDefinition::GetBoolDef();
	}
	else if (TypeLower == TEXT("vector2"))
	{
		TypeDef = FNiagaraTypeDefinition::GetVec2Def();
	}
	else if (TypeLower == TEXT("vector") || TypeLower == TEXT("vector3"))
	{
		TypeDef = FNiagaraTypeDefinition::GetVec3Def();
	}
	else if (TypeLower == TEXT("vector4"))
	{
		TypeDef = FNiagaraTypeDefinition::GetVec4Def();
	}
	else if (TypeLower == TEXT("color") || TypeLower == TEXT("linearcolor"))
	{
		TypeDef = FNiagaraTypeDefinition::GetColorDef();
	}
	else if (TypeLower == TEXT("string"))
	{
		OutWarn = FString::Printf(TEXT("当前UE版本不支持Niagara String类型: %s"), *FullName);
		return false;
	}
	else
	{
		OutWarn = FString::Printf(TEXT("不支持导入类型: %s (%s)"), *FullName, *InJson.Type);
		return false;
	}

	FNiagaraVariable Var(TypeDef, FName(*FullName));
	const bool bExists = Store.IndexOf(Var) != INDEX_NONE;
	if (!bExists && bAddMissing)
	{
		Store.AddParameter(Var, true);
	}
	else if (!bExists && !bAddMissing)
	{
		OutWarn = FString::Printf(TEXT("系统缺少参数: %s"), *FullName);
		return false;
	}

	if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
	{
		Store.SetParameterValue(static_cast<float>(InJson.NumberValue), Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
	{
		Store.SetParameterValue(InJson.IntValue, Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
	{
		Store.SetParameterValue(InJson.BoolValue, Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
	{
		Store.SetParameterValue(InJson.Vector2Value, Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
	{
		Store.SetParameterValue(InJson.Vector3Value, Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
	{
		Store.SetParameterValue(InJson.Vector4Value, Var);
		return true;
	}
	if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
	{
		Store.SetParameterValue(InJson.ColorValue, Var);
		return true;
	}

	OutWarn = FString::Printf(TEXT("设置失败: %s"), *FullName);
	return false;
}

#endif

bool UUEIRTask6JsonBPLibrary::ExportNiagaraSystemUserParametersToJsonFile(const FString& NiagaraSystemObjectPath, const FString& OutputFile, FString& OutJson, FString& OutReportText)
{
	OutJson.Reset();
	OutReportText.Reset();

	OutReportText += UEIRTask6_BuildNiagaraReportHeader(TEXT("UEIR Task6 Niagara User Parameters Export Report"));
	OutReportText += FString::Printf(TEXT("- SystemObjectPath: %s\n"), NiagaraSystemObjectPath.IsEmpty() ? TEXT("(empty)") : *NiagaraSystemObjectPath);
	OutReportText += FString::Printf(TEXT("- OutputFile: %s\n"), OutputFile.IsEmpty() ? TEXT("(empty)") : *OutputFile);
	OutReportText += TEXT("\n");

#if UEIR_WITH_NIAGARA
	if (NiagaraSystemObjectPath.IsEmpty() || OutputFile.IsEmpty())
	{
		OutReportText += TEXT("参数无效\n");
		return false;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *NiagaraSystemObjectPath);
	if (!IsValid(System))
	{
		OutReportText += TEXT("系统无法加载\n");
		return false;
	}

	FUEIRNiagaraSystemUserParametersPayloadJson Payload;
	Payload.SystemObjectPath = System->GetPathName();

	const FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
	TArray<FNiagaraVariable> Vars;
	Store.GetParameters(Vars);

	int32 Exported = 0;
	int32 Unsupported = 0;

	Payload.UserParameters.Reserve(Vars.Num());
	for (const FNiagaraVariable& Var : Vars)
	{
		FUEIRNiagaraUserParameterJson J;
		FString Warn;
		if (UEIRTask6_NiagaraTryReadParam(Store, Var, J, Warn))
		{
			Payload.UserParameters.Add(MoveTemp(J));
			++Exported;
		}
		else
		{
			++Unsupported;
			if (!Warn.IsEmpty())
			{
				OutReportText += FString::Printf(TEXT("- %s\n"), *Warn);
			}
		}
	}

	OutReportText += TEXT("\n");
	OutReportText += FString::Printf(TEXT("- Exported: %d\n"), Exported);
	OutReportText += FString::Printf(TEXT("- Unsupported: %d\n"), Unsupported);
	OutReportText += TEXT("\n");

	if (!FJsonObjectConverter::UStructToJsonObjectString(Payload, OutJson, 0, 0, 0, nullptr, true))
	{
		OutReportText += TEXT("序列化JSON失败\n");
		return false;
	}

	if (!UEIRTask6_SaveStringToFileEnsureDir(OutputFile, OutJson))
	{
		OutReportText += TEXT("写文件失败\n");
		return false;
	}

	return true;
#else
	OutReportText += TEXT("Niagara未启用或不可用，无法导出\n");
	return false;
#endif
}

bool UUEIRTask6JsonBPLibrary::ImportNiagaraSystemUserParametersFromJsonFile(const FString& InputFile, const FString& NiagaraSystemObjectPath, bool bApplyToSystem, bool bAddMissingParameters, FString& OutReportText)
{
	OutReportText.Reset();
	OutReportText += UEIRTask6_BuildNiagaraReportHeader(TEXT("UEIR Task6 Niagara User Parameters Import Report"));
	OutReportText += FString::Printf(TEXT("- InputFile: %s\n"), InputFile.IsEmpty() ? TEXT("(empty)") : *InputFile);
	OutReportText += FString::Printf(TEXT("- SystemObjectPath: %s\n"), NiagaraSystemObjectPath.IsEmpty() ? TEXT("(empty)") : *NiagaraSystemObjectPath);
	OutReportText += FString::Printf(TEXT("- ApplyToSystem: %s\n"), bApplyToSystem ? TEXT("Yes") : TEXT("No"));
	OutReportText += FString::Printf(TEXT("- AddMissingParameters: %s\n"), bAddMissingParameters ? TEXT("Yes") : TEXT("No"));
	OutReportText += TEXT("\n");

	FString JsonString;
	if (!UEIRTask6_LoadFileToString(InputFile, JsonString))
	{
		OutReportText += TEXT("读取JSON失败\n");
		return false;
	}

	FUEIRNiagaraSystemUserParametersPayloadJson Payload;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &Payload, 0, 0))
	{
		OutReportText += TEXT("解析JSON失败\n");
		return false;
	}

	OutReportText += FString::Printf(TEXT("- JsonSystemObjectPath: %s\n"), Payload.SystemObjectPath.IsEmpty() ? TEXT("(empty)") : *Payload.SystemObjectPath);
	OutReportText += FString::Printf(TEXT("- ParamCount: %d\n"), Payload.UserParameters.Num());
	OutReportText += TEXT("\n");

	if (!bApplyToSystem)
	{
		for (const FUEIRNiagaraUserParameterJson& P : Payload.UserParameters)
		{
			OutReportText += FString::Printf(TEXT("- %s (%s)\n"), *P.Name, *P.Type);
		}
		return true;
	}

#if UEIR_WITH_NIAGARA
	if (NiagaraSystemObjectPath.IsEmpty())
	{
		OutReportText += TEXT("SystemObjectPath为空\n");
		return false;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *NiagaraSystemObjectPath);
	if (!IsValid(System))
	{
		OutReportText += TEXT("系统无法加载\n");
		return false;
	}

	System->Modify();

	FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();

	int32 Applied = 0;
	int32 Skipped = 0;

	for (const FUEIRNiagaraUserParameterJson& P : Payload.UserParameters)
	{
		FString Warn;
		if (UEIRTask6_NiagaraTrySetParam(Store, P, bAddMissingParameters, Warn))
		{
			++Applied;
		}
		else
		{
			++Skipped;
			if (!Warn.IsEmpty())
			{
				OutReportText += FString::Printf(TEXT("- %s\n"), *Warn);
			}
		}
	}

	System->MarkPackageDirty();
	System->RequestCompile(false);

	OutReportText += TEXT("\n");
	OutReportText += FString::Printf(TEXT("- Applied: %d\n"), Applied);
	OutReportText += FString::Printf(TEXT("- Skipped: %d\n"), Skipped);
	OutReportText += TEXT("\n");

	if (!UEIRTask6_SavePackageForAsset(System))
	{
		OutReportText += TEXT("保存Package失败\n");
		return false;
	}

	return true;
#else
	OutReportText += TEXT("Niagara未启用或不可用，无法写回系统\n");
	return false;
#endif
}
