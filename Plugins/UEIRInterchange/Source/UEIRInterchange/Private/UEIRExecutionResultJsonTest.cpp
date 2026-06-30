#include "UEIRInterchangeBPLibrary.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUEIRExecutionResultJsonRoundtripTest, "UEIRInterchange.M1.ExecutionResult.JsonRoundtrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEIRExecutionResultJsonRoundtripTest::RunTest(const FString& Parameters)
{
	const FString JsonFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEIRInterchange"), TEXT("Roundtrip"), TEXT("ExecutionResult.json"));
	IFileManager::Get().Delete(*JsonFile, false, true, true);

	FUEIRExecutionResultJson Result;
	Result.Mode = TEXT("TestMode");
	Result.bOk = true;
	Result.ExitCode = 0;
	Result.ProjectName = TEXT("TestProject");
	Result.PluginName = TEXT("UEIRInterchange");
	Result.PluginVersion = TEXT("0.0.0");
	Result.EngineVersion = TEXT("0.0.0");
	Result.StartedAtUtcIso8601 = TEXT("2026-01-01T00:00:00Z");
	Result.FinishedAtUtcIso8601 = TEXT("2026-01-01T00:00:01Z");
	Result.ReportFile = TEXT("D:/tmp/report.md");
	{
		FUEIRExecutionArtifactJson A;
		A.Type = TEXT("UEIRDocument");
		A.FilePath = TEXT("D:/tmp/export.json");
		A.ObjectPath = TEXT("/Game/BP/BP_A.BP_A");
		A.ClassPath = TEXT("/Script/Engine.Blueprint");
		Result.Artifacts.Add(A);
	}
	{
		FUEIRExecutionErrorJson E;
		E.Code = TEXT("Warning");
		E.Message = TEXT("Something happened");
		E.Context = TEXT("Context");
		Result.Errors.Add(E);
	}

	FString SavedJson;
	const bool bSaved = UUEIRInterchangeBPLibrary::SaveUEIRExecutionResultToJsonFile(Result, JsonFile, SavedJson);
	TestTrue(TEXT("SaveUEIRExecutionResultToJsonFile"), bSaved);
	TestTrue(TEXT("SavedJsonNotEmpty"), !SavedJson.IsEmpty());
	TestTrue(TEXT("JsonFileExists"), IFileManager::Get().FileExists(*JsonFile));

	FString ReloadedJson;
	const bool bLoaded = FFileHelper::LoadFileToString(ReloadedJson, *JsonFile);
	TestTrue(TEXT("LoadFileToString"), bLoaded);

	FUEIRExecutionResultJson Reloaded;
	const bool bParsed = FJsonObjectConverter::JsonObjectStringToUStruct(ReloadedJson, &Reloaded, 0, 0);
	TestTrue(TEXT("JsonParse"), bParsed);

	if (bParsed)
	{
		TestEqual(TEXT("Mode"), Reloaded.Mode, Result.Mode);
		TestEqual(TEXT("ExitCode"), Reloaded.ExitCode, Result.ExitCode);
		TestTrue(TEXT("ArtifactsCount"), Reloaded.Artifacts.Num() == 1);
		TestTrue(TEXT("ErrorsCount"), Reloaded.Errors.Num() == 1);
	}

	IFileManager::Get().Delete(*JsonFile, false, true, true);
	return true;
}

#endif

