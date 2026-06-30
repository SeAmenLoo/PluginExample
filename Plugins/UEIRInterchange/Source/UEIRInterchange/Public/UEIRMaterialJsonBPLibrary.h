#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "UEIRMaterialJsonBPLibrary.generated.h"

class UMaterial;

UCLASS()
class UEIRINTERCHANGE_API UUEIRMaterialJsonBPLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Material|JSON")
	static bool ExportMaterialToJsonFile(const UMaterial* Asset, const FString& OutputFile, FString& OutJson, FString& OutReportText);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Material|JSON")
	static bool ImportMaterialFromJsonFile(const FString& InputFile, bool bOverwriteExisting, UMaterial*& OutAsset, FString& OutObjectPath, FString& OutReportText);
};

