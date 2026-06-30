#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPath.h"

#include "UEIRExampleDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FUEIRExampleEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSoftObjectPath Reference;
};

USTRUCT()
struct FUEIRExampleEntryJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString Key;

	UPROPERTY()
	int32 Value = 0;

	UPROPERTY()
	FString Reference;
};

USTRUCT(BlueprintType)
struct FUEIRExampleData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Revision = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSoftObjectPath PrimaryReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRExampleEntry> Entries;
};

USTRUCT()
struct FUEIRExampleDataJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString Title;

	UPROPERTY()
	int32 Revision = 1;

	UPROPERTY()
	FString PrimaryReference;

	UPROPERTY()
	TArray<FUEIRExampleEntryJson> Entries;
};

USTRUCT()
struct FUEIRExampleDataAssetPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FString PackagePath;

	UPROPERTY()
	FString AssetName;

	UPROPERTY()
	FUEIRExampleData Data;
};

USTRUCT()
struct FUEIRExampleDataAssetPayloadJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString PackagePath;

	UPROPERTY()
	FString AssetName;

	UPROPERTY()
	FUEIRExampleDataJson Data;
};

UCLASS(BlueprintType)
class UEIRINTERCHANGE_API UUEIRExampleDataAsset final : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUEIRExampleData Data;
};
