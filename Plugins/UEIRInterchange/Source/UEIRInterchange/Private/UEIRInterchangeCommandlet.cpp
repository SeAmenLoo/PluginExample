#include "UEIRInterchangeCommandlet.h"

#include "UEIRInterchangeBPLibrary.h"
#include "UEIRMaterialJsonBPLibrary.h"
#include "UEIRTask6JsonBPLibrary.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

static FString UEIRInterchange_NormalizeOutDir(const FString& InOutDir)
{
	if (!InOutDir.IsEmpty())
	{
		return InOutDir;
	}
	return UUEIRInterchangeBPLibrary::GetDefaultUEIROutputDir();
}

static FString UEIRInterchange_MakeOutFile(const FString& OutDir, const FString& InOutFile, bool bIsReport)
{
	if (!InOutFile.IsEmpty())
	{
		return InOutFile;
	}

	if (bIsReport)
	{
		return FPaths::Combine(OutDir, FPaths::GetCleanFilename(UUEIRInterchangeBPLibrary::MakeDefaultUEIRReportFilePath()));
	}

	return FPaths::Combine(OutDir, FPaths::GetCleanFilename(UUEIRInterchangeBPLibrary::MakeDefaultUEIRExportFilePath()));
}

static TArray<FString> UEIRInterchange_ParseAssets(const FString& AssetsValue)
{
	TArray<FString> Result;
	if (AssetsValue.IsEmpty())
	{
		return Result;
	}

	AssetsValue.ParseIntoArray(Result, TEXT(";"), true);
	if (Result.Num() <= 1)
	{
		Result.Reset();
		AssetsValue.ParseIntoArray(Result, TEXT(","), true);
	}

	for (FString& Item : Result)
	{
		Item.TrimStartAndEndInline();
	}
	Result.RemoveAll([](const FString& Item) { return Item.IsEmpty(); });

	return Result;
}

static void UEIRInterchange_AddError(FUEIRExecutionResultJson& Result, const FString& Code, const FString& Message, const FString& Context = FString())
{
	FUEIRExecutionErrorJson E;
	E.Code = Code;
	E.Message = Message;
	E.Context = Context;
	Result.Errors.Add(E);
}

static void UEIRInterchange_FillBaseResult(FUEIRExecutionResultJson& Result, const FString& Mode, const FDateTime& StartedAtUtc)
{
	Result.Mode = Mode;
	Result.StartedAtUtcIso8601 = StartedAtUtc.ToIso8601();
	Result.EngineVersion = FEngineVersion::Current().ToString();
	Result.ProjectName = FApp::GetProjectName();
	Result.PluginName = TEXT("UEIRInterchange");

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UEIRInterchange"));
	if (Plugin.IsValid())
	{
		Result.PluginVersion = Plugin->GetDescriptor().VersionName;
	}
}

UUEIRInterchangeCommandlet::UUEIRInterchangeCommandlet()
{
	LogToConsole = true;
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	ShowErrorCount = true;
}

int32 UUEIRInterchangeCommandlet::Main(const FString& Params)
{
	const FDateTime StartedAtUtc = FDateTime::UtcNow();

	FString Mode;
	FParse::Value(*Params, TEXT("Mode="), Mode);

	FString OutResultJsonFile;
	FParse::Value(*Params, TEXT("OutResultJson="), OutResultJsonFile);

	FUEIRExecutionResultJson Result;
	UEIRInterchange_FillBaseResult(Result, Mode, StartedAtUtc);

	int32 ExitCode = 1;

	FString OutDir;
	FParse::Value(*Params, TEXT("OutDir="), OutDir);
	OutDir = UEIRInterchange_NormalizeOutDir(OutDir);

	IFileManager::Get().MakeDirectory(*OutDir, true);

	if (Mode.Equals(TEXT("Export"), ESearchCase::IgnoreCase))
	{
		FString AssetsValue;
		FParse::Value(*Params, TEXT("Assets="), AssetsValue);
		const TArray<FString> Assets = UEIRInterchange_ParseAssets(AssetsValue);
		if (Assets.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet Export失败: 未提供Assets= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("Export requires Assets="), TEXT("Assets="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		FString OutFile;
		FParse::Value(*Params, TEXT("OutFile="), OutFile);
		OutFile = UEIRInterchange_MakeOutFile(OutDir, OutFile, false);

		FUEIRDocument Doc;
		FString Json;
		const bool bOk = UUEIRInterchangeBPLibrary::ExportAssetsToUEIRFile(Assets, OutFile, Doc, Json);
		if (bOk)
		{
			UE_LOG(LogTemp, Display, TEXT("UEIR Export完成: %s (Items=%d)"), *OutFile, Doc.Items.Num());
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("UEIRDocument");
				A.FilePath = OutFile;
				Result.Artifacts.Add(A);
			}
			ExitCode = 0;
		}
		else
		{
			UEIRInterchange_AddError(Result, TEXT("ExportFailed"), TEXT("ExportAssetsToUEIRFile failed"), OutFile);
			ExitCode = 1;
		}
		goto UEIR_Interchange_Finalize;
	}

	if (Mode.Equals(TEXT("Report"), ESearchCase::IgnoreCase))
	{
		FString Input;
		FParse::Value(*Params, TEXT("Input="), Input);
		if (Input.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet Report失败: 未提供Input= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("Report requires Input="), TEXT("Input="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		FString OutFile;
		FParse::Value(*Params, TEXT("OutFile="), OutFile);
		OutFile = UEIRInterchange_MakeOutFile(OutDir, OutFile, true);

		FString ReportText;
		const bool bOk = UUEIRInterchangeBPLibrary::GenerateReportFromUEIRFile(Input, OutFile, ReportText);
		if (bOk)
		{
			UE_LOG(LogTemp, Display, TEXT("UEIR Report生成完成: %s"), *OutFile);
			Result.ReportFile = OutFile;
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("ReportMarkdown");
				A.FilePath = OutFile;
				Result.Artifacts.Add(A);
			}
			ExitCode = 0;
		}
		else
		{
			UEIRInterchange_AddError(Result, TEXT("ReportFailed"), TEXT("GenerateReportFromUEIRFile failed"), Input);
			ExitCode = 1;
		}
		goto UEIR_Interchange_Finalize;
	}

	if (Mode.Equals(TEXT("GenCpp"), ESearchCase::IgnoreCase))
	{
		FString BlueprintPath;
		FParse::Value(*Params, TEXT("Blueprint="), BlueprintPath);
		if (BlueprintPath.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet GenCpp失败: 未提供Blueprint= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("GenCpp requires Blueprint="), TEXT("Blueprint="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		FString FunctionName;
		FParse::Value(*Params, TEXT("Function="), FunctionName);
		if (FunctionName.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet GenCpp失败: 未提供Function= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("GenCpp requires Function="), TEXT("Function="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
		if (!IsValid(Blueprint))
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet GenCpp失败: Blueprint无法加载: %s"), *BlueprintPath);
			UEIRInterchange_AddError(Result, TEXT("LoadFailed"), TEXT("Blueprint load failed"), BlueprintPath);
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		const FString OutGenDir = FPaths::Combine(OutDir, TEXT("Generated"));
		IFileManager::Get().MakeDirectory(*OutGenDir, true);

		FString HeaderFile;
		FString CppFile;
		FString ReportMarkdown;
		const bool bOk = UUEIRInterchangeBPLibrary::GenerateCppFromBlueprintFunctionGraph(Blueprint, FunctionName, OutGenDir, HeaderFile, CppFile, ReportMarkdown);
		if (bOk)
		{
			UE_LOG(LogTemp, Display, TEXT("UEIR GenCpp完成: %s / %s"), *HeaderFile, *CppFile);
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("CppHeader");
				A.FilePath = HeaderFile;
				Result.Artifacts.Add(A);
			}
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("CppSource");
				A.FilePath = CppFile;
				Result.Artifacts.Add(A);
			}
			{
				const FString SafeBlueprint = FPaths::MakeValidFileName(Blueprint->GetName());
				const FString SafeFunc = FPaths::MakeValidFileName(FunctionName);
				const FString ReportFile = FPaths::Combine(OutGenDir, FString::Printf(TEXT("%s_%s.codegen.md"), *SafeBlueprint, *SafeFunc));

				Result.ReportFile = ReportFile;

				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("CodegenReportMarkdown");
				A.FilePath = ReportFile;
				Result.Artifacts.Add(A);
			}
			ExitCode = 0;
		}
		else
		{
			UEIRInterchange_AddError(Result, TEXT("GenCppFailed"), TEXT("GenerateCppFromBlueprintFunctionGraph failed"), BlueprintPath);
			ExitCode = 1;
		}
		goto UEIR_Interchange_Finalize;
	}

	if (Mode.Equals(TEXT("ExportMIC"), ESearchCase::IgnoreCase))
	{
		FString AssetPath;
		FParse::Value(*Params, TEXT("Asset="), AssetPath);
		if (AssetPath.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet ExportMIC失败: 未提供Asset= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("ExportMIC requires Asset="), TEXT("Asset="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		UMaterialInstanceConstant* Asset = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
		if (!IsValid(Asset))
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet ExportMIC失败: 资产无法加载: %s"), *AssetPath);
			UEIRInterchange_AddError(Result, TEXT("LoadFailed"), TEXT("MaterialInstanceConstant load failed"), AssetPath);
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		FString OutFile;
		FParse::Value(*Params, TEXT("OutFile="), OutFile);
		OutFile = UEIRInterchange_MakeOutFile(OutDir, OutFile, false);

		FString Json;
		const bool bOk = UUEIRTask6JsonBPLibrary::ExportMaterialInstanceConstantToJsonFile(Asset, OutFile, Json);
		if (bOk)
		{
			UE_LOG(LogTemp, Display, TEXT("ExportMIC完成: %s"), *OutFile);
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("MaterialInstanceConstantJson");
				A.FilePath = OutFile;
				A.ObjectPath = Asset->GetPathName();
				A.ClassPath = Asset->GetClass() ? Asset->GetClass()->GetPathName() : FString();
				Result.Artifacts.Add(A);
			}
			ExitCode = 0;
		}
		else
		{
			UEIRInterchange_AddError(Result, TEXT("ExportMICFailed"), TEXT("ExportMaterialInstanceConstantToJsonFile failed"), OutFile);
			ExitCode = 1;
		}
		goto UEIR_Interchange_Finalize;
	}

	if (Mode.Equals(TEXT("ImportMIC"), ESearchCase::IgnoreCase))
	{
		FString Input;
		FParse::Value(*Params, TEXT("Input="), Input);
		if (Input.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet ImportMIC失败: 未提供Input= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("ImportMIC requires Input="), TEXT("Input="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		bool bOverwrite = false;
		FParse::Bool(*Params, TEXT("Overwrite="), bOverwrite);

		UMaterialInstanceConstant* Imported = nullptr;
		FString ObjPath;
		FString Report;
		const bool bOk = UUEIRTask6JsonBPLibrary::ImportMaterialInstanceConstantFromJsonFile(Input, bOverwrite, Imported, ObjPath, Report);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
		if (bOk)
		{
			UE_LOG(LogTemp, Display, TEXT("ImportMIC完成: %s"), *ObjPath);
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("MaterialInstanceConstant");
				A.ObjectPath = ObjPath;
				A.ClassPath = Imported && Imported->GetClass() ? Imported->GetClass()->GetPathName() : FString();
				Result.Artifacts.Add(A);
			}
			ExitCode = 0;
		}
		else
		{
			UEIRInterchange_AddError(Result, TEXT("ImportMICFailed"), TEXT("ImportMaterialInstanceConstantFromJsonFile failed"), Input);
			ExitCode = 1;
		}
		goto UEIR_Interchange_Finalize;
	}

	if (Mode.Equals(TEXT("ExportNiagaraUserParams"), ESearchCase::IgnoreCase))
	{
		FString SystemPath;
		FParse::Value(*Params, TEXT("System="), SystemPath);
		if (SystemPath.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet ExportNiagaraUserParams失败: 未提供System= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("ExportNiagaraUserParams requires System="), TEXT("System="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		FString OutFile;
		FParse::Value(*Params, TEXT("OutFile="), OutFile);
		OutFile = UEIRInterchange_MakeOutFile(OutDir, OutFile, false);

		FString Json;
		FString Report;
		const bool bOk = UUEIRTask6JsonBPLibrary::ExportNiagaraSystemUserParametersToJsonFile(SystemPath, OutFile, Json, Report);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
		if (bOk)
		{
			UE_LOG(LogTemp, Display, TEXT("ExportNiagaraUserParams完成: %s"), *OutFile);
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("NiagaraUserParamsJson");
				A.FilePath = OutFile;
				A.ObjectPath = SystemPath;
				Result.Artifacts.Add(A);
			}
			ExitCode = 0;
		}
		else
		{
			UEIRInterchange_AddError(Result, TEXT("ExportNiagaraUserParamsFailed"), TEXT("ExportNiagaraSystemUserParametersToJsonFile failed"), SystemPath);
			ExitCode = 1;
		}
		goto UEIR_Interchange_Finalize;
	}

	if (Mode.Equals(TEXT("ImportNiagaraUserParams"), ESearchCase::IgnoreCase))
	{
		FString Input;
		FParse::Value(*Params, TEXT("Input="), Input);
		if (Input.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet ImportNiagaraUserParams失败: 未提供Input= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("ImportNiagaraUserParams requires Input="), TEXT("Input="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		FString SystemPath;
		FParse::Value(*Params, TEXT("System="), SystemPath);
		if (SystemPath.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet ImportNiagaraUserParams失败: 未提供System= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("ImportNiagaraUserParams requires System="), TEXT("System="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		bool bApply = true;
		FParse::Bool(*Params, TEXT("Apply="), bApply);

		bool bAddMissing = true;
		FParse::Bool(*Params, TEXT("AddMissing="), bAddMissing);

		FString Report;
		const bool bOk = UUEIRTask6JsonBPLibrary::ImportNiagaraSystemUserParametersFromJsonFile(Input, SystemPath, bApply, bAddMissing, Report);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
		if (bOk)
		{
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("NiagaraSystem");
				A.ObjectPath = SystemPath;
				Result.Artifacts.Add(A);
			}
			ExitCode = 0;
		}
		else
		{
			UEIRInterchange_AddError(Result, TEXT("ImportNiagaraUserParamsFailed"), TEXT("ImportNiagaraSystemUserParametersFromJsonFile failed"), Input);
			ExitCode = 1;
		}
		goto UEIR_Interchange_Finalize;
	}

	if (Mode.Equals(TEXT("ExportMaterial"), ESearchCase::IgnoreCase))
	{
		FString AssetPath;
		FParse::Value(*Params, TEXT("Asset="), AssetPath);
		if (AssetPath.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet ExportMaterial失败: 未提供Asset= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("ExportMaterial requires Asset="), TEXT("Asset="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		UMaterial* Asset = LoadObject<UMaterial>(nullptr, *AssetPath);
		if (!IsValid(Asset))
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet ExportMaterial失败: 资产无法加载: %s"), *AssetPath);
			UEIRInterchange_AddError(Result, TEXT("LoadFailed"), TEXT("Material load failed"), AssetPath);
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		FString OutFile;
		FParse::Value(*Params, TEXT("OutFile="), OutFile);
		OutFile = UEIRInterchange_MakeOutFile(OutDir, OutFile, false);

		FString Json;
		FString Report;
		const bool bOk = UUEIRMaterialJsonBPLibrary::ExportMaterialToJsonFile(Asset, OutFile, Json, Report);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
		if (bOk)
		{
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("MaterialJson");
				A.FilePath = OutFile;
				A.ObjectPath = Asset->GetPathName();
				A.ClassPath = Asset->GetClass() ? Asset->GetClass()->GetPathName() : FString();
				Result.Artifacts.Add(A);
			}
			ExitCode = 0;
		}
		else
		{
			UEIRInterchange_AddError(Result, TEXT("ExportMaterialFailed"), TEXT("ExportMaterialToJsonFile failed"), AssetPath);
			ExitCode = 1;
		}
		goto UEIR_Interchange_Finalize;
	}

	if (Mode.Equals(TEXT("ImportMaterial"), ESearchCase::IgnoreCase))
	{
		FString Input;
		FParse::Value(*Params, TEXT("Input="), Input);
		if (Input.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet ImportMaterial失败: 未提供Input= 参数"));
			UEIRInterchange_AddError(Result, TEXT("MissingParam"), TEXT("ImportMaterial requires Input="), TEXT("Input="));
			ExitCode = 1;
			goto UEIR_Interchange_Finalize;
		}

		bool bOverwrite = false;
		FParse::Bool(*Params, TEXT("Overwrite="), bOverwrite);

		UMaterial* Imported = nullptr;
		FString ObjPath;
		FString Report;
		const bool bOk = UUEIRMaterialJsonBPLibrary::ImportMaterialFromJsonFile(Input, bOverwrite, Imported, ObjPath, Report);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
		if (bOk)
		{
			{
				FUEIRExecutionArtifactJson A;
				A.Type = TEXT("Material");
				A.ObjectPath = ObjPath;
				A.ClassPath = Imported && Imported->GetClass() ? Imported->GetClass()->GetPathName() : FString();
				Result.Artifacts.Add(A);
			}
			ExitCode = 0;
		}
		else
		{
			UEIRInterchange_AddError(Result, TEXT("ImportMaterialFailed"), TEXT("ImportMaterialFromJsonFile failed"), Input);
			ExitCode = 1;
		}
		goto UEIR_Interchange_Finalize;
	}

	UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet: 未知Mode=%s (支持: Export/Report/GenCpp/ExportMIC/ImportMIC/ExportNiagaraUserParams/ImportNiagaraUserParams/ExportMaterial/ImportMaterial)"), *Mode);
	UEIRInterchange_AddError(Result, TEXT("UnknownMode"), TEXT("Unknown Mode"), Mode);
	ExitCode = 1;

UEIR_Interchange_Finalize:
	Result.ExitCode = ExitCode;
	Result.bOk = ExitCode == 0;
	Result.FinishedAtUtcIso8601 = FDateTime::UtcNow().ToIso8601();
	if (!OutResultJsonFile.IsEmpty())
	{
		FString OutJson;
		const bool bWrote = UUEIRInterchangeBPLibrary::SaveUEIRExecutionResultToJsonFile(Result, OutResultJsonFile, OutJson);
		if (!bWrote)
		{
			UE_LOG(LogTemp, Error, TEXT("UEIRInterchangeCommandlet: 写结果JSON失败: %s"), *OutResultJsonFile);
			UEIRInterchange_AddError(Result, TEXT("WriteResultFailed"), TEXT("Failed to write OutResultJson"), OutResultJsonFile);

			if (ExitCode == 0)
			{
				ExitCode = 1;
			}
		}
	}
	return ExitCode;
}
