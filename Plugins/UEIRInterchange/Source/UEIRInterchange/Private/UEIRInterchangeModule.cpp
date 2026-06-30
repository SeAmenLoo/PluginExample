#include "UEIRInterchangeModule.h"

#include "UEIRInterchangeBPLibrary.h"
#include "UEIRMaterialJsonBPLibrary.h"

#include "Modules/ModuleManager.h"

#include "ContentBrowserModule.h"
#include "HAL/FileManager.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDesktopPlatform.h"
#include "Interfaces/IPluginManager.h"
#include "IContentBrowserSingleton.h"
#include "Materials/Material.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

static void UEIRInterchange_Notify(const FText& Text, SNotificationItem::ECompletionState State)
{
	FNotificationInfo Info(Text);
	Info.ExpireDuration = 5.0f;

	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(State);
	}
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

static bool UEIRInterchange_PromptAndSaveResultJson(const FUEIRExecutionResultJson& Result, const FString& DefaultResultFile)
{
	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("是否同时保存结构化结果JSON？")));
	if (Choice != EAppReturnType::Yes)
	{
		return false;
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return false;
	}

	const FString DefaultDir = FPaths::GetPath(DefaultResultFile);
	const FString DefaultFile = FPaths::GetCleanFilename(DefaultResultFile);
	void* ParentWindowHandle = const_cast<void*>(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr));

	TArray<FString> SelectedFiles;
	const bool bPicked = DesktopPlatform->SaveFileDialog(
		ParentWindowHandle,
		TEXT("UEIR Result JSON"),
		DefaultDir,
		DefaultFile,
		TEXT("UEIR Result JSON (*.json)|*.json|All Files (*.*)|*.*"),
		0,
		SelectedFiles);

	if (!bPicked || SelectedFiles.IsEmpty())
	{
		return false;
	}

	FString OutJson;
	const bool bOk = UUEIRInterchangeBPLibrary::SaveUEIRExecutionResultToJsonFile(Result, SelectedFiles[0], OutJson);
	if (bOk)
	{
		UEIRInterchange_Notify(FText::FromString(FString::Printf(TEXT("结果JSON已保存：%s"), *FPaths::ConvertRelativePathToFull(SelectedFiles[0]))), SNotificationItem::CS_Success);
	}
	return bOk;
}

void FUEIRInterchangeModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUEIRInterchangeModule::RegisterMenus));
}

void FUEIRInterchangeModule::ShutdownModule()
{
	if (UToolMenus::TryGet())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}
}

void FUEIRInterchangeModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
	{
		FToolMenuSection& Section = ToolsMenu->AddSection(TEXT("UEIRInterchange"), FText::FromString(TEXT("UEIR")));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.ExportSelectedAssets"),
			FText::FromString(TEXT("\u5bfc\u51fa\u6240\u9009\u8d44\u4ea7\u4e3a UEIR JSON")),
			FText::FromString(TEXT("\u4ee5\u8d44\u4ea7\u8def\u5f84\u6620\u5c04\u5230\u78c1\u76d8\u76ee\u5f55\uff0c\u9010\u4e2a\u5bfc\u51fa UEIR JSON\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::ExportSelectedAssetsToDefaultLocation)));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.ExportSelectedAssetsSingleFile"),
			FText::FromString(TEXT("\u5bfc\u51fa\u6240\u9009\u8d44\u4ea7\u4e3a UEIR JSON\uff08\u5355\u6587\u4ef6\uff09")),
			FText::FromString(TEXT("\u5c06\u9009\u4e2d\u8d44\u4ea7\u5408\u5e76\u4e3a\u5355\u4e2a UEIR JSON \u6587\u4ef6\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::ExportSelectedAssetsToDefaultLocationSingleFile)));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.GenerateReportFromUEIRJson"),
			FText::FromString(TEXT("\u4ece UEIR JSON \u751f\u6210\u62a5\u544a")),
			FText::FromString(TEXT("\u9009\u62e9\u4e00\u4e2a UEIR JSON \u6587\u4ef6\u5e76\u751f\u6210 Markdown \u62a5\u544a\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::GenerateReportFromUEIRJsonFile)));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.ExportSelectedMaterialsToJson"),
			FText::FromString(TEXT("\u5bfc\u51fa\u6240\u9009\u6750\u8d28\u4e3a Material JSON")),
			FText::FromString(TEXT("\u5bfc\u51fa\u6240\u9009 UMaterial \u4e3a\u57df\u4e13\u7528 JSON\uff08\u5305\u542b\u90e8\u5206\u8282\u70b9\u5b50\u96c6\u4e0e\u8fde\u7ebf\uff09\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::ExportSelectedMaterialsToDefaultLocation)));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.ImportMaterialFromJson"),
			FText::FromString(TEXT("\u4ece Material JSON \u5bfc\u5165/\u8986\u76d6\u6750\u8d28")),
			FText::FromString(TEXT("\u9009\u62e9\u4e00\u4e2a Material JSON \u6587\u4ef6\u5e76\u521b\u5efa\u6216\u8986\u76d6\u5bf9\u5e94\u6750\u8d28\u8d44\u4ea7\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::ImportMaterialFromJsonFile)));
	}

	if (UToolMenu* AssetContextMenu = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu")))
	{
		FToolMenuSection& Section = AssetContextMenu->AddSection(TEXT("UEIRInterchange"), FText::FromString(TEXT("UEIR")));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.ExportSelectedAssets_CB"),
			FText::FromString(TEXT("\u5bfc\u51fa\u6240\u9009\u8d44\u4ea7\u4e3a UEIR JSON")),
			FText::FromString(TEXT("\u4ee5\u8d44\u4ea7\u8def\u5f84\u6620\u5c04\u5230\u78c1\u76d8\u76ee\u5f55\uff0c\u9010\u4e2a\u5bfc\u51fa UEIR JSON\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::ExportSelectedAssetsToDefaultLocation)));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.ExportSelectedAssetsSingleFile_CB"),
			FText::FromString(TEXT("\u5bfc\u51fa\u6240\u9009\u8d44\u4ea7\u4e3a UEIR JSON\uff08\u5355\u6587\u4ef6\uff09")),
			FText::FromString(TEXT("\u5c06\u9009\u4e2d\u8d44\u4ea7\u5408\u5e76\u4e3a\u5355\u4e2a UEIR JSON \u6587\u4ef6\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::ExportSelectedAssetsToDefaultLocationSingleFile)));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.GenerateReportFromUEIRJson_CB"),
			FText::FromString(TEXT("\u4ece UEIR JSON \u751f\u6210\u62a5\u544a")),
			FText::FromString(TEXT("\u9009\u62e9\u4e00\u4e2a UEIR JSON \u6587\u4ef6\u5e76\u751f\u6210 Markdown \u62a5\u544a\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::GenerateReportFromUEIRJsonFile)));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.ExportSelectedMaterialsToJson_CB"),
			FText::FromString(TEXT("\u5bfc\u51fa\u6240\u9009\u6750\u8d28\u4e3a Material JSON")),
			FText::FromString(TEXT("\u5bfc\u51fa\u6240\u9009 UMaterial \u4e3a\u57df\u4e13\u7528 JSON\uff08\u5305\u542b\u90e8\u5206\u8282\u70b9\u5b50\u96c6\u4e0e\u8fde\u7ebf\uff09\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::ExportSelectedMaterialsToDefaultLocation)));

		Section.AddMenuEntry(
			TEXT("UEIRInterchange.ImportMaterialFromJson_CB"),
			FText::FromString(TEXT("\u4ece Material JSON \u5bfc\u5165/\u8986\u76d6\u6750\u8d28")),
			FText::FromString(TEXT("\u9009\u62e9\u4e00\u4e2a Material JSON \u6587\u4ef6\u5e76\u521b\u5efa\u6216\u8986\u76d6\u5bf9\u5e94\u6750\u8d28\u8d44\u4ea7\u3002")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FUEIRInterchangeModule::ImportMaterialFromJsonFile)));
	}
}

void FUEIRInterchangeModule::ExportSelectedAssetsToDefaultLocation()
{
	const FDateTime StartedAtUtc = FDateTime::UtcNow();
	const FString ExportRootDir = UUEIRInterchangeBPLibrary::GetDefaultUEIRExportRootDir();
	TArray<FString> Files;
	FString Error;
	const bool bOk = UUEIRInterchangeBPLibrary::ExportSelectedAssetsToUEIRFiles(ExportRootDir, Files, Error);
	if (bOk)
	{
		UEIRInterchange_Notify(FText::FromString(FString::Printf(TEXT("UEIR \u5bfc\u51fa\u5b8c\u6210\uff1a%d \u4e2a\u6587\u4ef6 (%s)"), Files.Num(), *FPaths::ConvertRelativePathToFull(ExportRootDir))), SNotificationItem::CS_Success);

		FUEIRExecutionResultJson Result;
		UEIRInterchange_FillBaseResult(Result, TEXT("Editor.ExportSelectedAssets"), StartedAtUtc);
		Result.ExitCode = 0;
		Result.bOk = true;
		Result.FinishedAtUtcIso8601 = FDateTime::UtcNow().ToIso8601();
		for (const FString& File : Files)
		{
			FUEIRExecutionArtifactJson A;
			A.Type = TEXT("UEIRDocument");
			A.FilePath = File;
			Result.Artifacts.Add(A);
		}
		UEIRInterchange_PromptAndSaveResultJson(Result, FPaths::Combine(ExportRootDir, TEXT("ueir_export.result.json")));
		return;
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error.IsEmpty() ? TEXT("UEIR \u5bfc\u51fa\u5931\u8d25") : Error));
}

void FUEIRInterchangeModule::ExportSelectedAssetsToDefaultLocationSingleFile()
{
	const FDateTime StartedAtUtc = FDateTime::UtcNow();
	const FString OutputFile = UUEIRInterchangeBPLibrary::MakeDefaultUEIRExportFilePath();

	FUEIRDocument Doc;
	FString Json;
	const bool bOk = UUEIRInterchangeBPLibrary::ExportSelectedAssetsToUEIRFile(OutputFile, Doc, Json);
	if (bOk)
	{
		UEIRInterchange_Notify(FText::FromString(FString::Printf(TEXT("UEIR \u5bfc\u51fa\u5b8c\u6210\uff1a%s"), *FPaths::ConvertRelativePathToFull(OutputFile))), SNotificationItem::CS_Success);

		FUEIRExecutionResultJson Result;
		UEIRInterchange_FillBaseResult(Result, TEXT("Editor.ExportSelectedAssetsSingleFile"), StartedAtUtc);
		Result.ExitCode = 0;
		Result.bOk = true;
		Result.FinishedAtUtcIso8601 = FDateTime::UtcNow().ToIso8601();
		{
			FUEIRExecutionArtifactJson A;
			A.Type = TEXT("UEIRDocument");
			A.FilePath = OutputFile;
			Result.Artifacts.Add(A);
		}
		UEIRInterchange_PromptAndSaveResultJson(Result, FPaths::Combine(FPaths::GetPath(OutputFile), FPaths::GetBaseFilename(OutputFile) + TEXT(".result.json")));
		return;
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("UEIR \u5bfc\u51fa\u5931\u8d25\uff1a\u8bf7\u786e\u8ba4\u5df2\u5728 Content Browser \u9009\u4e2d\u81f3\u5c11\u4e00\u4e2a\u53ef\u52a0\u8f7d\u8d44\u4ea7\u3002")));
}

void FUEIRInterchangeModule::GenerateReportFromUEIRJsonFile()
{
	const FDateTime StartedAtUtc = FDateTime::UtcNow();
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("\u65e0\u6cd5\u6253\u5f00\u6587\u4ef6\u9009\u62e9\u5668\uff1aDesktopPlatform \u4e0d\u53ef\u7528\u3002")));
		return;
	}

	const FString DefaultDir = UUEIRInterchangeBPLibrary::GetDefaultUEIROutputDir();
	void* ParentWindowHandle = const_cast<void*>(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr));

	TArray<FString> SelectedFiles;
	const bool bPicked = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("UEIR JSON"),
		DefaultDir,
		TEXT(""),
		TEXT("UEIR JSON (*.json)|*.json|All Files (*.*)|*.*"),
		0,
		SelectedFiles);

	if (!bPicked || SelectedFiles.IsEmpty())
	{
		return;
	}

	const FString InputFile = SelectedFiles[0];
	const FString OutputReportFile = FPaths::ChangeExtension(InputFile, TEXT("md"));

	FString ReportText;
	const bool bOk = UUEIRInterchangeBPLibrary::GenerateReportFromUEIRFile(InputFile, OutputReportFile, ReportText);
	if (bOk)
	{
		UEIRInterchange_Notify(FText::FromString(FString::Printf(TEXT("UEIR \u62a5\u544a\u751f\u6210\u5b8c\u6210\uff1a%s"), *FPaths::ConvertRelativePathToFull(OutputReportFile))), SNotificationItem::CS_Success);

		FUEIRExecutionResultJson Result;
		UEIRInterchange_FillBaseResult(Result, TEXT("Editor.GenerateReportFromUEIRJsonFile"), StartedAtUtc);
		Result.ExitCode = 0;
		Result.bOk = true;
		Result.ReportFile = OutputReportFile;
		Result.FinishedAtUtcIso8601 = FDateTime::UtcNow().ToIso8601();
		{
			FUEIRExecutionArtifactJson A;
			A.Type = TEXT("ReportMarkdown");
			A.FilePath = OutputReportFile;
			Result.Artifacts.Add(A);
		}
		UEIRInterchange_PromptAndSaveResultJson(Result, FPaths::Combine(FPaths::GetPath(OutputReportFile), FPaths::GetBaseFilename(OutputReportFile) + TEXT(".result.json")));
		return;
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("UEIR \u62a5\u544a\u751f\u6210\u5931\u8d25\uff1a\u8bf7\u786e\u8ba4\u8f93\u5165\u6587\u4ef6\u4e3a\u6709\u6548\u7684 UEIR JSON\u3002")));
}

void FUEIRInterchangeModule::ExportSelectedMaterialsToDefaultLocation()
{
	const FDateTime StartedAtUtc = FDateTime::UtcNow();

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	TArray<UMaterial*> Materials;
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (UMaterial* M = Cast<UMaterial>(Asset.GetAsset()))
		{
			Materials.Add(M);
		}
	}

	if (Materials.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("\u672a\u9009\u4e2d\u4efb\u4f55 UMaterial \u8d44\u4ea7\u3002")));
		return;
	}

	const FString OutDir = FPaths::Combine(UUEIRInterchangeBPLibrary::GetDefaultUEIROutputDir(), TEXT("Material"));
	IFileManager::Get().MakeDirectory(*OutDir, true);

	TArray<FString> OutFiles;
	OutFiles.Reserve(Materials.Num());

	for (UMaterial* M : Materials)
	{
		if (!IsValid(M))
		{
			continue;
		}

		const FString SafeName = FPaths::MakeValidFileName(M->GetName());
		const FString OutFile = FPaths::Combine(OutDir, SafeName + TEXT(".material.json"));

		FString Json;
		FString Report;
		if (!UUEIRMaterialJsonBPLibrary::ExportMaterialToJsonFile(M, OutFile, Json, Report))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Report.IsEmpty() ? TEXT("\u5bfc\u51fa\u6750\u8d28\u5931\u8d25") : Report));
			continue;
		}

		OutFiles.Add(OutFile);
	}

	if (OutFiles.IsEmpty())
	{
		return;
	}

	UEIRInterchange_Notify(FText::FromString(FString::Printf(TEXT("\u6750\u8d28JSON \u5bfc\u51fa\u5b8c\u6210\uff1a%d \u4e2a\u6587\u4ef6 (%s)"), OutFiles.Num(), *FPaths::ConvertRelativePathToFull(OutDir))), SNotificationItem::CS_Success);

	FUEIRExecutionResultJson Result;
	UEIRInterchange_FillBaseResult(Result, TEXT("Editor.ExportSelectedMaterials"), StartedAtUtc);
	Result.ExitCode = 0;
	Result.bOk = true;
	Result.FinishedAtUtcIso8601 = FDateTime::UtcNow().ToIso8601();
	for (const FString& File : OutFiles)
	{
		FUEIRExecutionArtifactJson A;
		A.Type = TEXT("MaterialJson");
		A.FilePath = File;
		Result.Artifacts.Add(A);
	}
	UEIRInterchange_PromptAndSaveResultJson(Result, FPaths::Combine(OutDir, TEXT("material_export.result.json")));
}

void FUEIRInterchangeModule::ImportMaterialFromJsonFile()
{
	const FDateTime StartedAtUtc = FDateTime::UtcNow();

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("\u65e0\u6cd5\u6253\u5f00\u6587\u4ef6\u9009\u62e9\u5668\uff1aDesktopPlatform \u4e0d\u53ef\u7528\u3002")));
		return;
	}

	const FString DefaultDir = FPaths::Combine(UUEIRInterchangeBPLibrary::GetDefaultUEIROutputDir(), TEXT("Material"));
	void* ParentWindowHandle = const_cast<void*>(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr));

	TArray<FString> SelectedFiles;
	const bool bPicked = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Material JSON"),
		DefaultDir,
		TEXT(""),
		TEXT("Material JSON (*.json)|*.json|All Files (*.*)|*.*"),
		0,
		SelectedFiles);

	if (!bPicked || SelectedFiles.IsEmpty())
	{
		return;
	}

	const EAppReturnType::Type OverwriteChoice = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT("\u5982\u679c\u5bf9\u5e94\u6750\u8d28\u5df2\u5b58\u5728\uff0c\u662f\u5426\u8986\u76d6\uff1f")));
	const bool bOverwrite = OverwriteChoice == EAppReturnType::Yes;

	UMaterial* Imported = nullptr;
	FString ObjPath;
	FString Report;
	const bool bOk = UUEIRMaterialJsonBPLibrary::ImportMaterialFromJsonFile(SelectedFiles[0], bOverwrite, Imported, ObjPath, Report);
	if (bOk)
	{
		UEIRInterchange_Notify(FText::FromString(FString::Printf(TEXT("\u6750\u8d28\u5bfc\u5165\u5b8c\u6210\uff1a%s"), *ObjPath)), SNotificationItem::CS_Success);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Report.IsEmpty() ? TEXT("\u6750\u8d28\u5bfc\u5165\u5931\u8d25") : Report));
		return;
	}

	FUEIRExecutionResultJson Result;
	UEIRInterchange_FillBaseResult(Result, TEXT("Editor.ImportMaterialFromJson"), StartedAtUtc);
	Result.ExitCode = 0;
	Result.bOk = true;
	Result.FinishedAtUtcIso8601 = FDateTime::UtcNow().ToIso8601();
	{
		FUEIRExecutionArtifactJson A;
		A.Type = TEXT("Material");
		A.ObjectPath = ObjPath;
		A.ClassPath = Imported && Imported->GetClass() ? Imported->GetClass()->GetPathName() : FString();
		Result.Artifacts.Add(A);
	}
	UEIRInterchange_PromptAndSaveResultJson(Result, FPaths::Combine(DefaultDir, TEXT("material_import.result.json")));
}

IMPLEMENT_MODULE(FUEIRInterchangeModule, UEIRInterchange)
