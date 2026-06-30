#include "UEIRMaterialJsonBPLibrary.h"

#include "UEIRMaterialJsonTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstance.h"
#include "Engine/Texture.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

static UObject* UEIRMaterial_GetEditorOnlyDataObj(const UMaterial* Material)
{
	if (!IsValid(Material))
	{
		return nullptr;
	}

#if WITH_EDITORONLY_DATA
	auto* EditorOnly = Material->GetEditorOnlyData();
	return const_cast<UObject*>(reinterpret_cast<const UObject*>(EditorOnly));
#else
	return nullptr;
#endif
}

static FString UEIRMaterial_EnumToString(const UEnum* Enum, int64 Value)
{
	return Enum ? Enum->GetNameStringByValue(Value) : FString();
}

static bool UEIRMaterial_GetEditorExpressionsArray(UObject* EditorOnlyData, FArrayProperty*& OutArrayProp, void*& OutArrayPtr)
{
	OutArrayProp = nullptr;
	OutArrayPtr = nullptr;

	if (!IsValid(EditorOnlyData))
	{
		return false;
	}

	UClass* Cls = EditorOnlyData->GetClass();

	if (FArrayProperty* Direct = FindFProperty<FArrayProperty>(Cls, TEXT("Expressions")))
	{
		OutArrayProp = Direct;
		OutArrayPtr = Direct->ContainerPtrToValuePtr<void>(EditorOnlyData);
		return true;
	}

	if (FStructProperty* Collection = FindFProperty<FStructProperty>(Cls, TEXT("ExpressionCollection")))
	{
		void* CollectionPtr = Collection->ContainerPtrToValuePtr<void>(EditorOnlyData);
		if (FArrayProperty* Inner = FindFProperty<FArrayProperty>(Collection->Struct, TEXT("Expressions")))
		{
			OutArrayProp = Inner;
			OutArrayPtr = Inner->ContainerPtrToValuePtr<void>(CollectionPtr);
			return true;
		}
	}

	return false;
}

static void UEIRMaterial_GatherExpressions(const UMaterial* Material, TArray<UMaterialExpression*>& OutExpressions)
{
	OutExpressions.Reset();

#if WITH_EDITORONLY_DATA
	UObject* EditorOnlyData = UEIRMaterial_GetEditorOnlyDataObj(Material);
	FArrayProperty* ArrayProp = nullptr;
	void* ArrayPtr = nullptr;
	if (UEIRMaterial_GetEditorExpressionsArray(EditorOnlyData, ArrayProp, ArrayPtr))
	{
		FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
		const int32 Num = Helper.Num();
		OutExpressions.Reserve(Num);

		const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner);
		if (!ObjProp)
		{
			return;
		}

		for (int32 I = 0; I < Num; ++I)
		{
			UObject* Obj = ObjProp->GetObjectPropertyValue(Helper.GetRawPtr(I));
			if (UMaterialExpression* Expr = Cast<UMaterialExpression>(Obj))
			{
				OutExpressions.Add(Expr);
			}
		}
		return;
	}
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
	if (IsValid(Material))
	{
		OutExpressions.Append(Material->Expressions);
	}
#endif
}

static bool UEIRMaterial_ResetExpressions(UMaterial* Material)
{
	if (!IsValid(Material))
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	UObject* EditorOnlyData = UEIRMaterial_GetEditorOnlyDataObj(Material);
	FArrayProperty* ArrayProp = nullptr;
	void* ArrayPtr = nullptr;
	if (UEIRMaterial_GetEditorExpressionsArray(EditorOnlyData, ArrayProp, ArrayPtr))
	{
		FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
		Helper.EmptyValues();
		return true;
	}
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
	Material->Expressions.Reset();
	return true;
#else
	return false;
#endif
}

static bool UEIRMaterial_AddExpression(UMaterial* Material, UMaterialExpression* Expr)
{
	if (!IsValid(Material) || !IsValid(Expr))
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	UObject* EditorOnlyData = UEIRMaterial_GetEditorOnlyDataObj(Material);
	FArrayProperty* ArrayProp = nullptr;
	void* ArrayPtr = nullptr;
	if (UEIRMaterial_GetEditorExpressionsArray(EditorOnlyData, ArrayProp, ArrayPtr))
	{
		FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
		const int32 Index = Helper.AddValue();

		const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner);
		if (!ObjProp)
		{
			return false;
		}
		ObjProp->SetObjectPropertyValue(Helper.GetRawPtr(Index), Expr);
		return true;
	}
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
	Material->Expressions.Add(Expr);
	return true;
#else
	return false;
#endif
}

static bool UEIRMaterial_GetRootConnection(const UMaterial* Material, const FString& PropertyName, UMaterialExpression*& OutExpr, int32& OutOutputIndex)
{
	OutExpr = nullptr;
	OutOutputIndex = 0;

	if (!IsValid(Material) || PropertyName.IsEmpty())
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	UObject* EditorOnlyData = UEIRMaterial_GetEditorOnlyDataObj(Material);
	if (!IsValid(EditorOnlyData))
	{
		return false;
	}

	UClass* Cls = EditorOnlyData->GetClass();
	FStructProperty* RootProp = FindFProperty<FStructProperty>(Cls, *PropertyName);
	if (!RootProp)
	{
		return false;
	}

	void* RootPtr = RootProp->ContainerPtrToValuePtr<void>(EditorOnlyData);
	FObjectPropertyBase* ExprProp = FindFProperty<FObjectPropertyBase>(RootProp->Struct, TEXT("Expression"));
	FIntProperty* OutIndexProp = FindFProperty<FIntProperty>(RootProp->Struct, TEXT("OutputIndex"));
	if (!ExprProp || !OutIndexProp)
	{
		return false;
	}

	UObject* Obj = ExprProp->GetObjectPropertyValue(ExprProp->ContainerPtrToValuePtr<void>(RootPtr));
	OutExpr = Cast<UMaterialExpression>(Obj);
	OutOutputIndex = OutIndexProp->GetPropertyValue(OutIndexProp->ContainerPtrToValuePtr<void>(RootPtr));
	return IsValid(OutExpr);
#else
	return false;
#endif
}

static bool UEIRMaterial_SetRootConnection(UMaterial* Material, const FString& PropertyName, UMaterialExpression* Expr, int32 OutputIndex)
{
	if (!IsValid(Material) || PropertyName.IsEmpty())
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	UObject* EditorOnlyData = UEIRMaterial_GetEditorOnlyDataObj(Material);
	if (!IsValid(EditorOnlyData))
	{
		return false;
	}

	UClass* Cls = EditorOnlyData->GetClass();
	FStructProperty* RootProp = FindFProperty<FStructProperty>(Cls, *PropertyName);
	if (!RootProp)
	{
		return false;
	}

	void* RootPtr = RootProp->ContainerPtrToValuePtr<void>(EditorOnlyData);
	FObjectPropertyBase* ExprProp = FindFProperty<FObjectPropertyBase>(RootProp->Struct, TEXT("Expression"));
	FIntProperty* OutIndexProp = FindFProperty<FIntProperty>(RootProp->Struct, TEXT("OutputIndex"));
	if (!ExprProp || !OutIndexProp)
	{
		return false;
	}

	ExprProp->SetObjectPropertyValue(ExprProp->ContainerPtrToValuePtr<void>(RootPtr), Expr);
	OutIndexProp->SetPropertyValue(OutIndexProp->ContainerPtrToValuePtr<void>(RootPtr), OutputIndex);
	return true;
#else
	return false;
#endif
}

static bool UEIRMaterial_SaveStringToFileEnsureDir(const FString& OutputFile, const FString& Text)
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

static bool UEIRMaterial_LoadFileToString(const FString& InputFile, FString& OutText)
{
	OutText.Reset();
	if (InputFile.IsEmpty())
	{
		return false;
	}
	const FString FullPath = FPaths::ConvertRelativePathToFull(InputFile);
	return FFileHelper::LoadFileToString(OutText, *FullPath);
}

static bool UEIRMaterial_SavePackageForAsset(UObject* Asset)
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

static FString UEIRMaterial_ExprId(const UMaterialExpression* Expr)
{
	if (!IsValid(Expr))
	{
		return FString();
	}
	return Expr->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens);
}

static void UEIRMaterial_AddReportLine(FString& Report, const FString& Line)
{
	if (!Report.IsEmpty())
	{
		Report.AppendChar(TEXT('\n'));
	}
	Report.Append(Line);
}

static bool UEIRMaterial_IsSupportedExpression(const UMaterialExpression* Expr)
{
	return
		Cast<UMaterialExpressionScalarParameter>(Expr) ||
		Cast<UMaterialExpressionVectorParameter>(Expr) ||
		Cast<UMaterialExpressionTextureSampleParameter2D>(Expr) ||
		Cast<UMaterialExpressionTextureSample>(Expr) ||
		Cast<UMaterialExpressionConstant>(Expr) ||
		Cast<UMaterialExpressionConstant3Vector>(Expr) ||
		Cast<UMaterialExpressionConstant4Vector>(Expr);
}

static bool UEIRMaterial_FillExpressionJson(const UMaterialExpression* Expr, FUEIRMaterialExpressionJson& OutJ)
{
	if (!IsValid(Expr))
	{
		return false;
	}

	if (!UEIRMaterial_IsSupportedExpression(Expr))
	{
		return false;
	}

	OutJ.Id = UEIRMaterial_ExprId(Expr);
	OutJ.ClassPath = Expr->GetClass() ? Expr->GetClass()->GetPathName() : FString();
	OutJ.EditorX = Expr->MaterialExpressionEditorX;
	OutJ.EditorY = Expr->MaterialExpressionEditorY;

	if (const UMaterialExpressionScalarParameter* P = Cast<UMaterialExpressionScalarParameter>(Expr))
	{
		OutJ.ParameterName = P->ParameterName.ToString();
		OutJ.NumberValue = P->DefaultValue;
		return true;
	}
	if (const UMaterialExpressionVectorParameter* P = Cast<UMaterialExpressionVectorParameter>(Expr))
	{
		OutJ.ParameterName = P->ParameterName.ToString();
		OutJ.ColorValue = P->DefaultValue;
		return true;
	}
	if (const UMaterialExpressionTextureSampleParameter2D* P = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
	{
		OutJ.ParameterName = P->ParameterName.ToString();
		OutJ.TextureObjectPath = IsValid(P->Texture) ? P->Texture->GetPathName() : FString();
		OutJ.SamplerType = UEIRMaterial_EnumToString(StaticEnum<EMaterialSamplerType>(), (int64)P->SamplerType);
		return true;
	}
	if (const UMaterialExpressionTextureSample* P = Cast<UMaterialExpressionTextureSample>(Expr))
	{
		OutJ.TextureObjectPath = IsValid(P->Texture) ? P->Texture->GetPathName() : FString();
		OutJ.SamplerType = UEIRMaterial_EnumToString(StaticEnum<EMaterialSamplerType>(), (int64)P->SamplerType);
		return true;
	}
	if (const UMaterialExpressionConstant* P = Cast<UMaterialExpressionConstant>(Expr))
	{
		OutJ.NumberValue = P->R;
		return true;
	}
	if (const UMaterialExpressionConstant3Vector* P = Cast<UMaterialExpressionConstant3Vector>(Expr))
	{
		OutJ.ColorValue = P->Constant;
		return true;
	}
	if (const UMaterialExpressionConstant4Vector* P = Cast<UMaterialExpressionConstant4Vector>(Expr))
	{
		OutJ.ColorValue = P->Constant;
		return true;
	}
	return false;
}

static void UEIRMaterial_CollectConnectionsForExpression(const UMaterialExpression* ToExpr, TArray<FUEIRMaterialConnectionJson>& OutConnections)
{
	if (!IsValid(ToExpr))
	{
		return;
	}

	for (TFieldIterator<FStructProperty> It(ToExpr->GetClass()); It; ++It)
	{
		const FStructProperty* Prop = *It;
		if (!Prop || !Prop->Struct || Prop->Struct->GetFName() != TEXT("ExpressionInput"))
		{
			continue;
		}

		const FExpressionInput* In = Prop->ContainerPtrToValuePtr<FExpressionInput>(ToExpr);
		if (!In || !IsValid(In->Expression))
		{
			continue;
		}

		const UMaterialExpression* FromExpr = In->Expression;
		if (!UEIRMaterial_IsSupportedExpression(FromExpr) || !UEIRMaterial_IsSupportedExpression(ToExpr))
		{
			continue;
		}

		FUEIRMaterialConnectionJson C;
		C.FromId = UEIRMaterial_ExprId(FromExpr);
		C.FromOutputIndex = In->OutputIndex;
		C.ToId = UEIRMaterial_ExprId(ToExpr);
		C.ToInputName = Prop->GetName();
		OutConnections.Add(MoveTemp(C));
	}
}

static void UEIRMaterial_AddRootConnIfValid(const FString& PropertyName, const FExpressionInput& Input, TArray<FUEIRMaterialRootConnectionJson>& OutRoot)
{
	if (!IsValid(Input.Expression))
	{
		return;
	}
	if (!UEIRMaterial_IsSupportedExpression(Input.Expression))
	{
		return;
	}

	FUEIRMaterialRootConnectionJson C;
	C.MaterialProperty = PropertyName;
	C.FromId = UEIRMaterial_ExprId(Input.Expression);
	C.FromOutputIndex = Input.OutputIndex;
	OutRoot.Add(MoveTemp(C));
}

bool UUEIRMaterialJsonBPLibrary::ExportMaterialToJsonFile(const UMaterial* Asset, const FString& OutputFile, FString& OutJson, FString& OutReportText)
{
	OutJson.Reset();
	OutReportText.Reset();

	if (!IsValid(Asset))
	{
		UEIRMaterial_AddReportLine(OutReportText, TEXT("ExportMaterialToJsonFile: Asset invalid"));
		return false;
	}

	const UPackage* Package = Asset->GetOutermost();

	FUEIRMaterialJsonFile File;
	File.Kind = TEXT("Asset");
	File.ObjectPath = Asset->GetPathName();
	File.ClassPath = Asset->GetClass() ? Asset->GetClass()->GetPathName() : FString();
	File.AssetName = Asset->GetName();
	File.PackagePath = IsValid(Package) ? FPackageName::GetLongPackagePath(Package->GetName()) : FString();

	File.Payload.bTwoSided = Asset->TwoSided;
	File.Payload.BlendMode = UEIRMaterial_EnumToString(StaticEnum<EBlendMode>(), (int64)Asset->BlendMode);
	File.Payload.ShadingModel = UEIRMaterial_EnumToString(StaticEnum<EMaterialShadingModel>(), (int64)Asset->GetShadingModels().GetFirstShadingModel());

	TArray<UMaterialExpression*> Expressions;
	UEIRMaterial_GatherExpressions(Asset, Expressions);

	int32 SupportedCount = 0;
	int32 SkippedCount = 0;
	for (const UMaterialExpression* Expr : Expressions)
	{
		if (!IsValid(Expr))
		{
			continue;
		}

		FUEIRMaterialExpressionJson J;
		if (UEIRMaterial_FillExpressionJson(Expr, J))
		{
			File.Payload.Expressions.Add(MoveTemp(J));
			SupportedCount++;
		}
		else
		{
			SkippedCount++;
		}
	}

	for (const UMaterialExpression* Expr : Expressions)
	{
		if (!IsValid(Expr))
		{
			continue;
		}
		UEIRMaterial_CollectConnectionsForExpression(Expr, File.Payload.Connections);
	}

	auto AddRoot = [&](const TCHAR* PropertyName)
	{
		UMaterialExpression* RootExpr = nullptr;
		int32 OutputIndex = 0;
		if (!UEIRMaterial_GetRootConnection(Asset, PropertyName, RootExpr, OutputIndex))
		{
			return;
		}
		if (!UEIRMaterial_IsSupportedExpression(RootExpr))
		{
			return;
		}
		FUEIRMaterialRootConnectionJson C;
		C.MaterialProperty = PropertyName;
		C.FromId = UEIRMaterial_ExprId(RootExpr);
		C.FromOutputIndex = OutputIndex;
		File.Payload.RootConnections.Add(MoveTemp(C));
	};

	AddRoot(TEXT("BaseColor"));
	AddRoot(TEXT("Metallic"));
	AddRoot(TEXT("Specular"));
	AddRoot(TEXT("Roughness"));
	AddRoot(TEXT("EmissiveColor"));
	AddRoot(TEXT("Opacity"));
	AddRoot(TEXT("OpacityMask"));
	AddRoot(TEXT("Normal"));

	if (!FJsonObjectConverter::UStructToJsonObjectString(File, OutJson, 0, 0, 0, nullptr, true))
	{
		UEIRMaterial_AddReportLine(OutReportText, TEXT("ExportMaterialToJsonFile: Failed to serialize json"));
		return false;
	}

	if (!UEIRMaterial_SaveStringToFileEnsureDir(OutputFile, OutJson))
	{
		UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("ExportMaterialToJsonFile: Failed to write file: %s"), *OutputFile));
		return false;
	}

	UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("ExportMaterialToJsonFile: OK")));
	UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("ObjectPath: %s"), *File.ObjectPath));
	UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("OutFile: %s"), *FPaths::ConvertRelativePathToFull(OutputFile)));
	UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("SupportedExpressions: %d"), SupportedCount));
	UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("SkippedExpressions: %d"), SkippedCount));
	return true;
}

static UMaterialExpression* UEIRMaterial_NewExpressionForClassPath(UMaterial* Material, const FString& ClassPath)
{
	if (!IsValid(Material) || ClassPath.IsEmpty())
	{
		return nullptr;
	}

	UClass* Cls = LoadObject<UClass>(nullptr, *ClassPath);
	if (!IsValid(Cls) || !Cls->IsChildOf(UMaterialExpression::StaticClass()))
	{
		return nullptr;
	}

	UMaterialExpression* Expr = NewObject<UMaterialExpression>(Material, Cls, NAME_None, RF_Transactional);
	if (IsValid(Expr))
	{
		Expr->MaterialExpressionGuid = FGuid::NewGuid();
	}
	return Expr;
}

static bool UEIRMaterial_ApplyExpressionJson(UMaterialExpression* Expr, const FUEIRMaterialExpressionJson& J)
{
	if (!IsValid(Expr))
	{
		return false;
	}

	if (!J.Id.IsEmpty())
	{
		FGuid Guid;
		if (FGuid::Parse(J.Id, Guid))
		{
			Expr->MaterialExpressionGuid = Guid;
		}
	}

	Expr->MaterialExpressionEditorX = J.EditorX;
	Expr->MaterialExpressionEditorY = J.EditorY;

	if (UMaterialExpressionScalarParameter* P = Cast<UMaterialExpressionScalarParameter>(Expr))
	{
		P->ParameterName = FName(*J.ParameterName);
		P->DefaultValue = (float)J.NumberValue;
		return true;
	}
	if (UMaterialExpressionVectorParameter* P = Cast<UMaterialExpressionVectorParameter>(Expr))
	{
		P->ParameterName = FName(*J.ParameterName);
		P->DefaultValue = J.ColorValue;
		return true;
	}
	if (UMaterialExpressionTextureSampleParameter2D* P = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
	{
		P->ParameterName = FName(*J.ParameterName);
		if (!J.TextureObjectPath.IsEmpty())
		{
			P->Texture = LoadObject<UTexture>(nullptr, *J.TextureObjectPath);
		}
		if (const UEnum* Enum = StaticEnum<EMaterialSamplerType>())
		{
			const int64 Value = Enum->GetValueByNameString(J.SamplerType);
			if (Value != INDEX_NONE)
			{
				P->SamplerType = (EMaterialSamplerType)Value;
			}
		}
		return true;
	}
	if (UMaterialExpressionTextureSample* P = Cast<UMaterialExpressionTextureSample>(Expr))
	{
		if (!J.TextureObjectPath.IsEmpty())
		{
			P->Texture = LoadObject<UTexture>(nullptr, *J.TextureObjectPath);
		}
		if (const UEnum* Enum = StaticEnum<EMaterialSamplerType>())
		{
			const int64 Value = Enum->GetValueByNameString(J.SamplerType);
			if (Value != INDEX_NONE)
			{
				P->SamplerType = (EMaterialSamplerType)Value;
			}
		}
		return true;
	}
	if (UMaterialExpressionConstant* P = Cast<UMaterialExpressionConstant>(Expr))
	{
		P->R = (float)J.NumberValue;
		return true;
	}
	if (UMaterialExpressionConstant3Vector* P = Cast<UMaterialExpressionConstant3Vector>(Expr))
	{
		P->Constant = J.ColorValue;
		return true;
	}
	if (UMaterialExpressionConstant4Vector* P = Cast<UMaterialExpressionConstant4Vector>(Expr))
	{
		P->Constant = J.ColorValue;
		return true;
	}
	return true;
}

static bool UEIRMaterial_ApplyConnectionJson(UMaterialExpression* ToExpr, const FString& ToInputName, UMaterialExpression* FromExpr, int32 FromOutputIndex)
{
	if (!IsValid(ToExpr) || !IsValid(FromExpr))
	{
		return false;
	}

	for (TFieldIterator<FStructProperty> It(ToExpr->GetClass()); It; ++It)
	{
		const FStructProperty* Prop = *It;
		if (!Prop || !Prop->Struct || Prop->Struct->GetFName() != TEXT("ExpressionInput"))
		{
			continue;
		}
		if (!ToInputName.IsEmpty() && Prop->GetName() != ToInputName)
		{
			continue;
		}

		FExpressionInput* In = Prop->ContainerPtrToValuePtr<FExpressionInput>(ToExpr);
		if (!In)
		{
			continue;
		}

		In->Expression = FromExpr;
		In->OutputIndex = FromOutputIndex;
		return true;
	}
	return false;
}

static void UEIRMaterial_ClearRootInputs(UMaterial* Material)
{
	if (!IsValid(Material))
	{
		return;
	}

	UEIRMaterial_SetRootConnection(Material, TEXT("BaseColor"), nullptr, 0);
	UEIRMaterial_SetRootConnection(Material, TEXT("Metallic"), nullptr, 0);
	UEIRMaterial_SetRootConnection(Material, TEXT("Specular"), nullptr, 0);
	UEIRMaterial_SetRootConnection(Material, TEXT("Roughness"), nullptr, 0);
	UEIRMaterial_SetRootConnection(Material, TEXT("EmissiveColor"), nullptr, 0);
	UEIRMaterial_SetRootConnection(Material, TEXT("Opacity"), nullptr, 0);
	UEIRMaterial_SetRootConnection(Material, TEXT("OpacityMask"), nullptr, 0);
	UEIRMaterial_SetRootConnection(Material, TEXT("Normal"), nullptr, 0);
}

static bool UEIRMaterial_ApplyRootConnection(UMaterial* Material, const FString& PropertyName, UMaterialExpression* FromExpr, int32 FromOutputIndex)
{
	if (!IsValid(Material) || !IsValid(FromExpr))
	{
		return false;
	}

	return UEIRMaterial_SetRootConnection(Material, PropertyName, FromExpr, FromOutputIndex);
}

bool UUEIRMaterialJsonBPLibrary::ImportMaterialFromJsonFile(const FString& InputFile, bool bOverwriteExisting, UMaterial*& OutAsset, FString& OutObjectPath, FString& OutReportText)
{
	OutAsset = nullptr;
	OutObjectPath.Reset();
	OutReportText.Reset();

	FString JsonText;
	if (!UEIRMaterial_LoadFileToString(InputFile, JsonText))
	{
		UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("ImportMaterialFromJsonFile: Failed to read file: %s"), *InputFile));
		return false;
	}

	FUEIRMaterialJsonFile File;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonText, &File, 0, 0))
	{
		UEIRMaterial_AddReportLine(OutReportText, TEXT("ImportMaterialFromJsonFile: Invalid json"));
		return false;
	}

	if (File.AssetName.IsEmpty() || File.PackagePath.IsEmpty())
	{
		UEIRMaterial_AddReportLine(OutReportText, TEXT("ImportMaterialFromJsonFile: AssetName/PackagePath missing"));
		return false;
	}

	const FString LongPackageName = FString::Printf(TEXT("%s/%s"), *File.PackagePath, *File.AssetName);
	const FString TargetObjectPath = FString::Printf(TEXT("%s.%s"), *LongPackageName, *File.AssetName);

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *TargetObjectPath);
	if (IsValid(Material))
	{
		if (!bOverwriteExisting)
		{
			UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("ImportMaterialFromJsonFile: Asset exists and overwrite disabled: %s"), *TargetObjectPath));
			return false;
		}
	}
	else
	{
		UPackage* Package = CreatePackage(*LongPackageName);
		if (!IsValid(Package))
		{
			UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("ImportMaterialFromJsonFile: CreatePackage failed: %s"), *LongPackageName));
			return false;
		}

		Material = NewObject<UMaterial>(Package, *File.AssetName, RF_Public | RF_Standalone);
		if (!IsValid(Material))
		{
			UEIRMaterial_AddReportLine(OutReportText, TEXT("ImportMaterialFromJsonFile: NewObject<UMaterial> failed"));
			return false;
		}

		FAssetRegistryModule::AssetCreated(Material);
	}

	Material->TwoSided = File.Payload.bTwoSided;
	UEIRMaterial_ClearRootInputs(Material);
	UEIRMaterial_ResetExpressions(Material);

	TMap<FString, UMaterialExpression*> ExprById;
	ExprById.Reserve(File.Payload.Expressions.Num());

	for (const FUEIRMaterialExpressionJson& J : File.Payload.Expressions)
	{
		if (J.ClassPath.IsEmpty())
		{
			continue;
		}

		UMaterialExpression* Expr = UEIRMaterial_NewExpressionForClassPath(Material, J.ClassPath);
		if (!IsValid(Expr))
		{
			UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("ImportMaterialFromJsonFile: Failed to create expr: %s"), *J.ClassPath));
			continue;
		}

		UEIRMaterial_ApplyExpressionJson(Expr, J);
		UEIRMaterial_AddExpression(Material, Expr);

		const FString Id = UEIRMaterial_ExprId(Expr);
		if (!Id.IsEmpty())
		{
			ExprById.Add(Id, Expr);
		}
	}

	int32 ConnOk = 0;
	int32 ConnFail = 0;
	for (const FUEIRMaterialConnectionJson& C : File.Payload.Connections)
	{
		UMaterialExpression** From = ExprById.Find(C.FromId);
		UMaterialExpression** To = ExprById.Find(C.ToId);
		if (!From || !To || !IsValid(*From) || !IsValid(*To))
		{
			ConnFail++;
			continue;
		}

		if (UEIRMaterial_ApplyConnectionJson(*To, C.ToInputName, *From, C.FromOutputIndex))
		{
			ConnOk++;
		}
		else
		{
			ConnFail++;
		}
	}

	int32 RootOk = 0;
	int32 RootFail = 0;
	for (const FUEIRMaterialRootConnectionJson& C : File.Payload.RootConnections)
	{
		UMaterialExpression** From = ExprById.Find(C.FromId);
		if (!From || !IsValid(*From))
		{
			RootFail++;
			continue;
		}

		if (UEIRMaterial_ApplyRootConnection(Material, C.MaterialProperty, *From, C.FromOutputIndex))
		{
			RootOk++;
		}
		else
		{
			RootFail++;
		}
	}

	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	const bool bSaved = UEIRMaterial_SavePackageForAsset(Material);

	OutAsset = Material;
	OutObjectPath = Material->GetPathName();

	UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("ImportMaterialFromJsonFile: %s"), bSaved ? TEXT("OK") : TEXT("SavedFailed")));
	UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("ObjectPath: %s"), *OutObjectPath));
	UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("Connections: ok=%d fail=%d"), ConnOk, ConnFail));
	UEIRMaterial_AddReportLine(OutReportText, FString::Printf(TEXT("RootConnections: ok=%d fail=%d"), RootOk, RootFail));
	return bSaved;
}
