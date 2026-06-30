#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "UEIRExampleDataAsset.h"

#include "UEIRDataAssetJsonBPLibrary.generated.h"

UCLASS()
class UEIRINTERCHANGE_API UUEIRDataAssetJsonBPLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|DataAsset|JSON")
	static bool ExportExampleDataAssetToJsonFile(const UUEIRExampleDataAsset* Asset, const FString& OutputFile, FString& OutJson);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|DataAsset|JSON")
	static bool ImportExampleDataAssetFromJsonFile(const FString& InputFile, bool bOverwriteExisting, UUEIRExampleDataAsset*& OutAsset, FString& OutObjectPath);
};
