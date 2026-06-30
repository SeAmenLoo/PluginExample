#pragma once

#include "CoreMinimal.h"

#include "UEIRExecutionResultTypes.generated.h"

USTRUCT()
struct FUEIRExecutionArtifactJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FString FilePath;

	UPROPERTY()
	FString ObjectPath;

	UPROPERTY()
	FString ClassPath;
};

USTRUCT()
struct FUEIRExecutionErrorJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString Code;

	UPROPERTY()
	FString Message;

	UPROPERTY()
	FString Context;
};

USTRUCT()
struct FUEIRExecutionResultJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString Mode;

	UPROPERTY()
	bool bOk = false;

	UPROPERTY()
	int32 ExitCode = 1;

	UPROPERTY()
	FString EngineVersion;

	UPROPERTY()
	FString ProjectName;

	UPROPERTY()
	FString PluginName;

	UPROPERTY()
	FString PluginVersion;

	UPROPERTY()
	FString StartedAtUtcIso8601;

	UPROPERTY()
	FString FinishedAtUtcIso8601;

	UPROPERTY()
	TArray<FUEIRExecutionArtifactJson> Artifacts;

	UPROPERTY()
	FString ReportFile;

	UPROPERTY()
	TArray<FUEIRExecutionErrorJson> Errors;
};

