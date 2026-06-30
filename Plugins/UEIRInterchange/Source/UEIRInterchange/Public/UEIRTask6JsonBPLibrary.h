#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "UEIRTask6JsonBPLibrary.generated.h"

class UMaterialInstanceConstant;

UCLASS()
class UEIRINTERCHANGE_API UUEIRTask6JsonBPLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Task6|Material|JSON")
	static bool ExportMaterialInstanceConstantToJsonFile(const UMaterialInstanceConstant* Asset, const FString& OutputFile, FString& OutJson);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Task6|Material|JSON")
	static bool ImportMaterialInstanceConstantFromJsonFile(const FString& InputFile, bool bOverwriteExisting, UMaterialInstanceConstant*& OutAsset, FString& OutObjectPath, FString& OutReportText);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Task6|Niagara|JSON")
	static bool ExportNiagaraSystemUserParametersToJsonFile(const FString& NiagaraSystemObjectPath, const FString& OutputFile, FString& OutJson, FString& OutReportText);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Task6|Niagara|JSON")
	static bool ImportNiagaraSystemUserParametersFromJsonFile(const FString& InputFile, const FString& NiagaraSystemObjectPath, bool bApplyToSystem, bool bAddMissingParameters, FString& OutReportText);
};

