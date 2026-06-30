#pragma once

#include "CoreMinimal.h"

#include "UEIRTask6JsonTypes.generated.h"

USTRUCT(BlueprintType)
struct FUEIRMaterialScalarParameterJson
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 0.0f;
};

USTRUCT(BlueprintType)
struct FUEIRMaterialVectorParameterJson
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Value = FLinearColor::Black;
};

USTRUCT(BlueprintType)
struct FUEIRMaterialTextureParameterJson
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TextureObjectPath;
};

USTRUCT(BlueprintType)
struct FUEIRMaterialInstanceConstantPayloadJson
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString AssetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PackagePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ParentObjectPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRMaterialScalarParameterJson> ScalarParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRMaterialVectorParameterJson> VectorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRMaterialTextureParameterJson> TextureParameters;
};

USTRUCT(BlueprintType)
struct FUEIRNiagaraUserParameterJson
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double NumberValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2f Vector2Value = FVector2f::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector3f Vector3Value = FVector3f::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector4f Vector4Value = FVector4f::Zero();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor ColorValue = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString StringValue;
};

USTRUCT(BlueprintType)
struct FUEIRNiagaraSystemUserParametersPayloadJson
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SystemObjectPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRNiagaraUserParameterJson> UserParameters;
};

