#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "UEIRTypes.h"
#include "UEIRExecutionResultTypes.h"

#include "UEIRInterchangeBPLibrary.generated.h"

UCLASS()
class UEIRINTERCHANGE_API UUEIRInterchangeBPLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "UEIR|Paths")
	static FString GetDefaultUEIROutputDir();

	UFUNCTION(BlueprintPure, Category = "UEIR|Paths")
	static FString GetDefaultUEIRExportRootDir();

	UFUNCTION(BlueprintPure, Category = "UEIR|Paths")
	static FString MakeDefaultUEIRExportFilePath();

	UFUNCTION(BlueprintPure, Category = "UEIR|Paths")
	static FString MakeDefaultUEIRReportFilePath();

	UFUNCTION(BlueprintPure, Category = "UEIR|Paths")
	static FString MakeUEIRExportFilePathForObjectPath(const FString& ObjectPath, const FString& ExportRootDir);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Export")
	static bool ExportSelectedAssetsToUEIRFile(const FString& OutputFile, FUEIRDocument& OutDocument, FString& OutJson);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Export")
	static bool ExportSelectedAssetsToUEIRFiles(const FString& ExportRootDir, TArray<FString>& OutFiles, FString& OutError);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Export")
	static bool ExportAssetsToUEIRFile(const TArray<FString>& ObjectPaths, const FString& OutputFile, FUEIRDocument& OutDocument, FString& OutJson);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|Report")
	static bool GenerateReportFromUEIRFile(const FString& InputUEIRJsonFile, const FString& OutputReportFile, FString& OutReportText);

	UFUNCTION(BlueprintPure, Category = "UEIR|Schema")
	static FString GetUEIRSchemaIdV1();

	UFUNCTION(BlueprintPure, Category = "UEIR|Schema")
	static FString GetUEIRSchemaFilePathV1();

	UFUNCTION(BlueprintPure, Category = "UEIR|Schema")
	static FString GetUEIRVersionV1();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|BlueprintGraph")
	static bool ExportBlueprintFunctionGraphToUEIR(const UBlueprint* Blueprint, const FString& FunctionGraphName, FUEIRBlueprintFunctionGraph& OutGraph, FUEIRBlueprintGraphConversionReport& OutReport);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|BlueprintGraph")
	static bool ImportUEIRBlueprintFunctionGraphToBlueprint(UBlueprint* TargetBlueprint, const FUEIRBlueprintFunctionGraph& Graph, bool bReplaceIfExists, FUEIRBlueprintGraphConversionReport& OutReport);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|BlueprintGraph")
	static bool CreateBlueprintAndImportUEIRBlueprintFunctionGraph(const FString& PackagePath, const FString& BlueprintName, const FString& ParentClassPath, const FUEIRBlueprintFunctionGraph& Graph, UBlueprint*& OutBlueprint, FString& OutObjectPath, FUEIRBlueprintGraphConversionReport& OutReport);

	UFUNCTION(BlueprintPure, Category = "UEIR|BlueprintGraph")
	static FString BlueprintGraphReportToMarkdown(const FUEIRBlueprintGraphConversionReport& Report);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|CodeGen")
	static bool GenerateBlueprintWrapperFunctionGraphFromNativeFunction(UBlueprint* TargetBlueprint, const FString& WrapperGraphName, const FString& NativeFunctionPath, bool bReplaceIfExists, FUEIRBlueprintGraphConversionReport& OutReport);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|CodeGen")
	static bool GenerateCppFromBlueprintFunctionGraph(const UBlueprint* Blueprint, const FString& FunctionGraphName, const FString& OutputDir, FString& OutHeaderFile, FString& OutCppFile, FString& OutReportMarkdown);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "UEIR|CodeGen")
	static bool GenerateCppFromUEIRBlueprintFunctionGraph(const FString& BlueprintName, const FString& FunctionGraphName, const FUEIRBlueprintFunctionGraph& Graph, const FString& OutputDir, FString& OutHeaderFile, FString& OutCppFile, FString& OutReportMarkdown);

	static bool SaveUEIRExecutionResultToJsonFile(const FUEIRExecutionResultJson& Result, const FString& OutputFile, FString& OutJson);
};
