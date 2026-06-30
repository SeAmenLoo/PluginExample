#pragma once

#include "CoreMinimal.h"

#include "UEIRMaterialJsonTypes.generated.h"

USTRUCT()
struct FUEIRMaterialExpressionJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString Id;

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	int32 EditorX = 0;

	UPROPERTY()
	int32 EditorY = 0;

	UPROPERTY()
	FString ParameterName;

	UPROPERTY()
	double NumberValue = 0.0;

	UPROPERTY()
	FLinearColor ColorValue = FLinearColor::Black;

	UPROPERTY()
	FString TextureObjectPath;

	UPROPERTY()
	FString SamplerType;
};

USTRUCT()
struct FUEIRMaterialConnectionJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString FromId;

	UPROPERTY()
	int32 FromOutputIndex = 0;

	UPROPERTY()
	FString ToId;

	UPROPERTY()
	FString ToInputName;
};

USTRUCT()
struct FUEIRMaterialRootConnectionJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString MaterialProperty;

	UPROPERTY()
	FString FromId;

	UPROPERTY()
	int32 FromOutputIndex = 0;
};

USTRUCT()
struct FUEIRMaterialPayloadJson
{
	GENERATED_BODY()

	UPROPERTY()
	bool bTwoSided = false;

	UPROPERTY()
	FString BlendMode;

	UPROPERTY()
	FString ShadingModel;

	UPROPERTY()
	TArray<FUEIRMaterialExpressionJson> Expressions;

	UPROPERTY()
	TArray<FUEIRMaterialConnectionJson> Connections;

	UPROPERTY()
	TArray<FUEIRMaterialRootConnectionJson> RootConnections;
};

USTRUCT()
struct FUEIRMaterialJsonFile
{
	GENERATED_BODY()

	UPROPERTY()
	FString Kind = TEXT("Asset");

	UPROPERTY()
	FString ObjectPath;

	UPROPERTY()
	FString ClassPath = TEXT("/Script/Engine.Material");

	UPROPERTY()
	FString AssetName;

	UPROPERTY()
	FString PackagePath;

	UPROPERTY()
	int32 FormatVersion = 1;

	UPROPERTY()
	FUEIRMaterialPayloadJson Payload;
};

