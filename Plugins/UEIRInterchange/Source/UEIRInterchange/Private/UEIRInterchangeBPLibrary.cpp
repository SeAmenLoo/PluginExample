#include "UEIRInterchangeBPLibrary.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "IContentBrowserSingleton.h"
#include "Interfaces/IPluginManager.h"
#include "JsonObjectConverter.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

static FString UEIRInterchange_GetPluginBaseDir()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UEIRInterchange"));
	return Plugin.IsValid() ? Plugin->GetBaseDir() : FString();
}

static FUEIRItem UEIRInterchange_BuildItemFromObject(UObject* Object)
{
	FUEIRItem Item;
	if (!IsValid(Object))
	{
		return Item;
	}

	Item.ObjectPath = Object->GetPathName();
	Item.ClassPath = Object->GetClass() ? Object->GetClass()->GetPathName() : FString();

	if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
	{
		Item.Kind = EUEIRItemKind::Blueprint;
		Item.Blueprint.ParentClassPath = Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString();

		for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
		{
			FUEIRVariable Var;
			Var.Name = VarDesc.VarName.ToString();
			Var.Type = VarDesc.VarType.PinCategory.ToString();

			switch (VarDesc.VarType.ContainerType)
			{
			case EPinContainerType::Array:
				Var.Container = TEXT("Array");
				break;
			case EPinContainerType::Set:
				Var.Container = TEXT("Set");
				break;
			case EPinContainerType::Map:
				Var.Container = TEXT("Map");
				break;
			default:
				Var.Container = TEXT("None");
				break;
			}

			if (VarDesc.VarType.PinSubCategoryObject.IsValid())
			{
				if (UObject* SubObj = VarDesc.VarType.PinSubCategoryObject.Get())
				{
					Var.SubTypeObjectPath = SubObj->GetPathName();
				}
			}

			Item.Blueprint.Variables.Add(MoveTemp(Var));
		}

		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (IsValid(Graph))
			{
				Item.Blueprint.FunctionGraphNames.Add(Graph->GetName());
			}
		}
	}
	else if (UClass* Class = Cast<UClass>(Object))
	{
		Item.Kind = EUEIRItemKind::Class;
		Item.ClassPath = Class->GetPathName();
		Item.ObjectPath = Class->GetPathName();
	}
	else
	{
		Item.Kind = EUEIRItemKind::Asset;
	}

	return Item;
}

static bool UEIRInterchange_WriteDocumentToFile(const FString& OutputFile, const FUEIRDocument& Document, FString& OutJson)
{
	if (OutputFile.IsEmpty())
	{
		return false;
	}

	if (!FJsonObjectConverter::UStructToJsonObjectString(Document, OutJson, 0, 0, 0, nullptr, true))
	{
		return false;
	}

	const FString FullPath = FPaths::ConvertRelativePathToFull(OutputFile);
	const FString Directory = FPaths::GetPath(FullPath);
	if (!Directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}
	return FFileHelper::SaveStringToFile(OutJson, *FullPath);
}

static FString UEIRInterchange_GetDefaultOutputDir()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ConversionReports"), TEXT("UEIR"));
}

FString UUEIRInterchangeBPLibrary::GetUEIRSchemaIdV1()
{
	return TEXT("ueir://schemas/v1/document.json");
}

FString UUEIRInterchangeBPLibrary::GetDefaultUEIROutputDir()
{
	return UEIRInterchange_GetDefaultOutputDir();
}

FString UUEIRInterchangeBPLibrary::GetDefaultUEIRExportRootDir()
{
	return FPaths::Combine(UEIRInterchange_GetDefaultOutputDir(), TEXT("Export"));
}

FString UUEIRInterchangeBPLibrary::MakeDefaultUEIRExportFilePath()
{
	return FPaths::Combine(UEIRInterchange_GetDefaultOutputDir(), TEXT("UEIRExport.json"));
}

FString UUEIRInterchangeBPLibrary::MakeDefaultUEIRReportFilePath()
{
	return FPaths::Combine(UEIRInterchange_GetDefaultOutputDir(), TEXT("UEIRReport.md"));
}

FString UUEIRInterchangeBPLibrary::MakeUEIRExportFilePathForObjectPath(const FString& ObjectPath, const FString& ExportRootDir)
{
	if (ObjectPath.IsEmpty())
	{
		return FString();
	}

	const FString RootDir = ExportRootDir.IsEmpty() ? GetDefaultUEIRExportRootDir() : ExportRootDir;
	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	if (PackageName.IsEmpty())
	{
		return FString();
	}

	FString LongPackagePath = FPackageName::GetLongPackagePath(PackageName);
	if (LongPackagePath.StartsWith(TEXT("/")))
	{
		LongPackagePath.RightChopInline(1);
	}

	const FString AssetName = FPackageName::GetShortName(PackageName);
	return FPaths::Combine(RootDir, LongPackagePath, FString::Printf(TEXT("%s.json"), *AssetName));
}

FString UUEIRInterchangeBPLibrary::GetUEIRSchemaFilePathV1()
{
	const FString BaseDir = UEIRInterchange_GetPluginBaseDir();
	if (BaseDir.IsEmpty())
	{
		return FString();
	}

	return FPaths::Combine(BaseDir, TEXT("Resources"), TEXT("Schemas"), TEXT("ueir_v1.schema.json"));
}

FString UUEIRInterchangeBPLibrary::GetUEIRVersionV1()
{
	return TEXT("1.0.0-draft");
}

bool UUEIRInterchangeBPLibrary::GenerateReportFromUEIRFile(const FString& InputUEIRJsonFile, const FString& OutputReportFile, FString& OutReportText)
{
	OutReportText.Reset();

	if (InputUEIRJsonFile.IsEmpty() || OutputReportFile.IsEmpty())
	{
		return false;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *InputUEIRJsonFile))
	{
		return false;
	}

	FUEIRDocument Doc;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonText, &Doc, 0, 0))
	{
		return false;
	}

	int32 AssetCount = 0;
	int32 BlueprintCount = 0;
	int32 ClassCount = 0;
	for (const FUEIRItem& Item : Doc.Items)
	{
		switch (Item.Kind)
		{
		case EUEIRItemKind::Blueprint:
			++BlueprintCount;
			break;
		case EUEIRItemKind::Class:
			++ClassCount;
			break;
		default:
			++AssetCount;
			break;
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	OutReportText += TEXT("# UEIR Conversion Report\n\n");
	OutReportText += FString::Printf(TEXT("- Input: %s\n"), *FPaths::ConvertRelativePathToFull(InputUEIRJsonFile));
	OutReportText += FString::Printf(TEXT("- ExportedAtUtc: %s\n"), *Doc.Meta.ExportedAtUtcIso8601);
	OutReportText += FString::Printf(TEXT("- EngineVersion: %s\n"), *Doc.Meta.EngineVersion);
	OutReportText += FString::Printf(TEXT("- ProjectName: %s\n"), *Doc.Meta.ProjectName);
	OutReportText += FString::Printf(TEXT("- SchemaId: %s\n"), *Doc.SchemaId);
	OutReportText += FString::Printf(TEXT("- Version: %s\n"), *Doc.Version);
	OutReportText += TEXT("\n");

	OutReportText += TEXT("## Summary\n\n");
	OutReportText += FString::Printf(TEXT("- TotalItems: %d\n"), Doc.Items.Num());
	OutReportText += FString::Printf(TEXT("- Assets: %d\n"), AssetCount);
	OutReportText += FString::Printf(TEXT("- Blueprints: %d\n"), BlueprintCount);
	OutReportText += FString::Printf(TEXT("- Classes: %d\n"), ClassCount);
	OutReportText += TEXT("\n");

	OutReportText += TEXT("## Items\n\n");
	OutReportText += TEXT("| Kind | ObjectPath | ExistsInProject | ClassPath | Details |\n");
	OutReportText += TEXT("| --- | --- | --- | --- | --- |\n");

	for (const FUEIRItem& Item : Doc.Items)
	{
		const FString KindString =
			Item.Kind == EUEIRItemKind::Blueprint ? TEXT("Blueprint") :
			Item.Kind == EUEIRItemKind::Class ? TEXT("Class") :
			TEXT("Asset");

		const FSoftObjectPath SoftPath(Item.ObjectPath);
		const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(SoftPath);
		const bool bExists = AssetData.IsValid();

		FString Details;
		if (Item.Kind == EUEIRItemKind::Blueprint)
		{
			Details = FString::Printf(TEXT("Parent=%s; Vars=%d; FuncGraphs=%d"), *Item.Blueprint.ParentClassPath, Item.Blueprint.Variables.Num(), Item.Blueprint.FunctionGraphNames.Num());
		}

		OutReportText += FString::Printf(TEXT("| %s | %s | %s | %s | %s |\n"),
			*KindString,
			*Item.ObjectPath.Replace(TEXT("|"), TEXT("\\|")),
			bExists ? TEXT("Yes") : TEXT("No"),
			*Item.ClassPath.Replace(TEXT("|"), TEXT("\\|")),
			*Details.Replace(TEXT("|"), TEXT("\\|")));
	}

	const FString FullPath = FPaths::ConvertRelativePathToFull(OutputReportFile);
	const FString Directory = FPaths::GetPath(FullPath);
	if (!Directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}
	return FFileHelper::SaveStringToFile(OutReportText, *FullPath);
}

bool UUEIRInterchangeBPLibrary::ExportSelectedAssetsToUEIRFile(const FString& OutputFile, FUEIRDocument& OutDocument, FString& OutJson)
{
	OutDocument = FUEIRDocument();
	OutJson.Reset();

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	TArray<UObject*> Objects;
	Objects.Reserve(SelectedAssets.Num());
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (UObject* Obj = Asset.GetAsset())
		{
			Objects.Add(Obj);
		}
	}

	TArray<FString> Paths;
	Paths.Reserve(Objects.Num());
	for (UObject* Obj : Objects)
	{
		if (IsValid(Obj))
		{
			Paths.Add(Obj->GetPathName());
		}
	}

	return ExportAssetsToUEIRFile(Paths, OutputFile, OutDocument, OutJson);
}

bool UUEIRInterchangeBPLibrary::ExportSelectedAssetsToUEIRFiles(const FString& ExportRootDir, TArray<FString>& OutFiles, FString& OutError)
{
	OutFiles.Reset();
	OutError.Reset();

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
	if (SelectedAssets.IsEmpty())
	{
		OutError = TEXT("未选择任何资产");
		return false;
	}

	TArray<FString> ObjectPaths;
	ObjectPaths.Reserve(SelectedAssets.Num());
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (UObject* Obj = Asset.GetAsset())
		{
			ObjectPaths.Add(Obj->GetPathName());
		}
	}

	const FString RootDir = ExportRootDir.IsEmpty() ? GetDefaultUEIRExportRootDir() : ExportRootDir;
	IFileManager::Get().MakeDirectory(*RootDir, true);

	bool bAllOk = true;
	for (const FString& ObjectPath : ObjectPaths)
	{
		const FString OutFile = MakeUEIRExportFilePathForObjectPath(ObjectPath, RootDir);
		if (OutFile.IsEmpty())
		{
			bAllOk = false;
			continue;
		}

		FUEIRDocument Doc;
		FString Json;
		if (!ExportAssetsToUEIRFile({ ObjectPath }, OutFile, Doc, Json))
		{
			bAllOk = false;
			continue;
		}

		OutFiles.Add(OutFile);
	}

	if (!bAllOk && OutFiles.IsEmpty())
	{
		OutError = TEXT("导出失败：未能导出任何资产");
		return false;
	}

	return bAllOk;
}

bool UUEIRInterchangeBPLibrary::ExportAssetsToUEIRFile(const TArray<FString>& ObjectPaths, const FString& OutputFile, FUEIRDocument& OutDocument, FString& OutJson)
{
	OutDocument = FUEIRDocument();
	OutJson.Reset();

	OutDocument.SchemaId = GetUEIRSchemaIdV1();
	OutDocument.Version = GetUEIRVersionV1();

	OutDocument.Meta.ExportedAtUtcIso8601 = FDateTime::UtcNow().ToIso8601();
	OutDocument.Meta.EngineVersion = FEngineVersion::Current().ToString();
	OutDocument.Meta.ProjectName = FApp::GetProjectName();
	OutDocument.Meta.PluginName = TEXT("UEIRInterchange");
	OutDocument.Meta.PluginVersion = TEXT("0.1.0");

	for (const FString& Path : ObjectPaths)
	{
		if (Path.IsEmpty())
		{
			continue;
		}

		UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
		if (!IsValid(Obj))
		{
			continue;
		}

		OutDocument.Items.Add(UEIRInterchange_BuildItemFromObject(Obj));
	}

	return UEIRInterchange_WriteDocumentToFile(OutputFile, OutDocument, OutJson);
}

static FString UEIRInterchange_NodeClassNameFromPath(const FString& ClassPath)
{
	int32 DotIndex = INDEX_NONE;
	if (!ClassPath.FindLastChar(TEXT('.'), DotIndex))
	{
		return ClassPath;
	}
	return ClassPath.Mid(DotIndex + 1);
}

static bool UEIRInterchange_IsSupportedK2NodeClassName(const FString& ClassName)
{
	return
		ClassName == TEXT("K2Node_CallFunction") ||
		ClassName == TEXT("K2Node_IfThenElse") ||
		ClassName == TEXT("K2Node_FunctionEntry") ||
		ClassName == TEXT("K2Node_FunctionResult") ||
		ClassName == TEXT("K2Node_ReturnNode") ||
		ClassName == TEXT("K2Node_VariableGet") ||
		ClassName == TEXT("K2Node_VariableSet");
}

static FString UEIRInterchange_PinDirectionToString(EEdGraphPinDirection Direction)
{
	return Direction == EGPD_Output ? TEXT("Output") : TEXT("Input");
}

static EEdGraphPinDirection UEIRInterchange_PinDirectionFromString(const FString& Direction)
{
	return Direction.Equals(TEXT("Output"), ESearchCase::IgnoreCase) ? EGPD_Output : EGPD_Input;
}

static FString UEIRInterchange_ContainerToString(EPinContainerType ContainerType)
{
	switch (ContainerType)
	{
	case EPinContainerType::Array:
		return TEXT("Array");
	case EPinContainerType::Set:
		return TEXT("Set");
	case EPinContainerType::Map:
		return TEXT("Map");
	default:
		return TEXT("None");
	}
}

static FUEIRGraphPinType UEIRInterchange_ExportPinType(const FEdGraphPinType& PinType)
{
	FUEIRGraphPinType Out;
	Out.Category = PinType.PinCategory.ToString();
	Out.SubCategory = PinType.PinSubCategory.ToString();
	Out.Container = UEIRInterchange_ContainerToString(PinType.ContainerType);
	Out.bIsReference = PinType.bIsReference;
	Out.bIsConst = PinType.bIsConst;
	if (PinType.PinSubCategoryObject.IsValid())
	{
		if (UObject* Obj = PinType.PinSubCategoryObject.Get())
		{
			Out.SubCategoryObjectPath = Obj->GetPathName();
		}
	}
	return Out;
}

static FUEIRGraphPin UEIRInterchange_ExportPin(const UEdGraphPin* Pin)
{
	FUEIRGraphPin Out;
	if (!Pin)
	{
		return Out;
	}

	Out.Name = Pin->PinName.ToString();
	Out.Direction = UEIRInterchange_PinDirectionToString(Pin->Direction);
	Out.Type = UEIRInterchange_ExportPinType(Pin->PinType);
	Out.DefaultValue = Pin->DefaultValue;
	Out.DefaultObjectPath = IsValid(Pin->DefaultObject) ? Pin->DefaultObject->GetPathName() : FString();
	Out.DefaultText = Pin->DefaultTextValue.ToString();
	return Out;
}

static void UEIRInterchange_AddUnique(TArray<FString>& InOutArray, const FString& Value)
{
	if (Value.IsEmpty())
	{
		return;
	}
	if (!InOutArray.Contains(Value))
	{
		InOutArray.Add(Value);
	}
}

static FString UEIRInterchange_MakeEdgeKey(const FUEIRGraphEdge& Edge)
{
	return FString::Printf(TEXT("%s|%s|%s|%s"), *Edge.From.NodeId, *Edge.From.PinName, *Edge.To.NodeId, *Edge.To.PinName);
}

static UEdGraphPin* UEIRInterchange_FindPinByNameAndDirection(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}

	UEdGraphPin* Pin = Node->FindPin(FName(*PinName), Direction);
	if (Pin)
	{
		return Pin;
	}

	for (UEdGraphPin* P : Node->Pins)
	{
		if (!P)
		{
			continue;
		}
		if (P->Direction == Direction && P->PinName.ToString() == PinName)
		{
			return P;
		}
	}
	return nullptr;
}

static void UEIRInterchange_ApplyPinDefaults(UEdGraphPin* Pin, const FUEIRGraphPin& PinIR)
{
	if (!Pin)
	{
		return;
	}

	Pin->DefaultValue = PinIR.DefaultValue;
	Pin->DefaultTextValue = FText::FromString(PinIR.DefaultText);

	Pin->DefaultObject = nullptr;
	if (!PinIR.DefaultObjectPath.IsEmpty())
	{
		UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *PinIR.DefaultObjectPath);
		if (IsValid(Obj))
		{
			Pin->DefaultObject = Obj;
		}
	}
}

static bool UEIRInterchange_FindBlueprintFunctionGraph(const UBlueprint* Blueprint, const FString& FunctionGraphName, UEdGraph*& OutGraph)
{
	OutGraph = nullptr;
	if (!IsValid(Blueprint) || FunctionGraphName.IsEmpty())
	{
		return false;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (IsValid(Graph) && Graph->GetName() == FunctionGraphName)
		{
			OutGraph = Graph;
			return true;
		}
	}
	return false;
}

static bool UEIRInterchange_IsEntryNode(const UEdGraphNode* Node)
{
	return !!Cast<UK2Node_FunctionEntry>(Node);
}

static bool UEIRInterchange_IsReturnNode(const UEdGraphNode* Node)
{
	return !!Cast<UK2Node_FunctionResult>(Node);
}

static void UEIRInterchange_ExportNodePayload(const UEdGraphNode* Node, FUEIRGraphNode& OutNodeIR)
{
	if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
	{
		if (UFunction* Func = Call->GetTargetFunction())
		{
			OutNodeIR.CallFunctionPath = Func->GetPathName();
		}
	}
	else if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
	{
		OutNodeIR.VariableName = Get->GetVarName().ToString();
	}
	else if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
	{
		OutNodeIR.VariableName = Set->GetVarName().ToString();
	}
}

static bool UEIRInterchange_IsSupportedNode(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return false;
	}

	return
		Cast<UK2Node_CallFunction>(Node) ||
		Cast<UK2Node_IfThenElse>(Node) ||
		Cast<UK2Node_FunctionEntry>(Node) ||
		Cast<UK2Node_FunctionResult>(Node) ||
		Cast<UK2Node_VariableGet>(Node) ||
		Cast<UK2Node_VariableSet>(Node);
}

static FString UEIRInterchange_GetNodeId(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}

	if (Node->NodeGuid.IsValid())
	{
		return Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
	}
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
}

static FUEIRBlueprintGraphConversionReport UEIRInterchange_MakeEmptyReport()
{
	FUEIRBlueprintGraphConversionReport R;
	R.bSuccess = false;
	return R;
}

bool UUEIRInterchangeBPLibrary::ExportBlueprintFunctionGraphToUEIR(const UBlueprint* Blueprint, const FString& FunctionGraphName, FUEIRBlueprintFunctionGraph& OutGraph, FUEIRBlueprintGraphConversionReport& OutReport)
{
	OutGraph = FUEIRBlueprintFunctionGraph();
	OutReport = UEIRInterchange_MakeEmptyReport();

	UEdGraph* Graph = nullptr;
	if (!UEIRInterchange_FindBlueprintFunctionGraph(Blueprint, FunctionGraphName, Graph))
	{
		OutReport.Errors.Add(TEXT("Blueprint or FunctionGraphName invalid, or graph not found"));
		return false;
	}

	OutGraph.GraphName = Graph->GetName();

	TMap<const UEdGraphNode*, FString> NodeIdMap;
	NodeIdMap.Reserve(Graph->Nodes.Num());

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!IsValid(Node))
		{
			continue;
		}

		if (!UEIRInterchange_IsSupportedNode(Node))
		{
			const FString ClassPath = Node->GetClass() ? Node->GetClass()->GetPathName() : FString();
			UEIRInterchange_AddUnique(OutReport.UnsupportedNodeClasses, ClassPath);
			continue;
		}

		FUEIRGraphNode NodeIR;
		NodeIR.Id = UEIRInterchange_GetNodeId(Node);
		NodeIR.ClassPath = Node->GetClass() ? Node->GetClass()->GetPathName() : FString();
		NodeIR.PosX = Node->NodePosX;
		NodeIR.PosY = Node->NodePosY;
		NodeIR.Pins.Reserve(Node->Pins.Num());
		for (UEdGraphPin* Pin : Node->Pins)
		{
			NodeIR.Pins.Add(UEIRInterchange_ExportPin(Pin));
		}
		UEIRInterchange_ExportNodePayload(Node, NodeIR);

		NodeIdMap.Add(Node, NodeIR.Id);
		OutGraph.Nodes.Add(MoveTemp(NodeIR));
	}

	TSet<FString> EdgeKeySet;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!IsValid(Node) || !UEIRInterchange_IsSupportedNode(Node))
		{
			continue;
		}

		const FString* FromNodeId = NodeIdMap.Find(Node);
		if (!FromNodeId)
		{
			continue;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}

			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (!Linked || !Linked->GetOwningNode())
				{
					continue;
				}

				UEdGraphNode* OtherNode = Linked->GetOwningNode();
				if (!IsValid(OtherNode) || !UEIRInterchange_IsSupportedNode(OtherNode))
				{
					continue;
				}

				const FString* ToNodeId = NodeIdMap.Find(OtherNode);
				if (!ToNodeId)
				{
					continue;
				}

				FUEIRGraphEdge Edge;
				const bool bForward = Pin->Direction == EGPD_Output && Linked->Direction == EGPD_Input;
				const bool bBackward = Pin->Direction == EGPD_Input && Linked->Direction == EGPD_Output;

				if (bForward)
				{
					Edge.From.NodeId = *FromNodeId;
					Edge.From.PinName = Pin->PinName.ToString();
					Edge.To.NodeId = *ToNodeId;
					Edge.To.PinName = Linked->PinName.ToString();
				}
				else if (bBackward)
				{
					Edge.From.NodeId = *ToNodeId;
					Edge.From.PinName = Linked->PinName.ToString();
					Edge.To.NodeId = *FromNodeId;
					Edge.To.PinName = Pin->PinName.ToString();
				}
				else
				{
					const bool bOrder = (*FromNodeId < *ToNodeId) || (*FromNodeId == *ToNodeId && Pin->PinName.ToString() < Linked->PinName.ToString());
					if (bOrder)
					{
						Edge.From.NodeId = *FromNodeId;
						Edge.From.PinName = Pin->PinName.ToString();
						Edge.To.NodeId = *ToNodeId;
						Edge.To.PinName = Linked->PinName.ToString();
					}
					else
					{
						Edge.From.NodeId = *ToNodeId;
						Edge.From.PinName = Linked->PinName.ToString();
						Edge.To.NodeId = *FromNodeId;
						Edge.To.PinName = Pin->PinName.ToString();
					}
				}

				const FString Key = UEIRInterchange_MakeEdgeKey(Edge);
				if (!EdgeKeySet.Contains(Key))
				{
					EdgeKeySet.Add(Key);
					OutGraph.Edges.Add(MoveTemp(Edge));
				}
			}
		}
	}

	if (OutGraph.Nodes.Num() == 0)
	{
		OutReport.Errors.Add(TEXT("No supported nodes exported"));
		OutReport.bSuccess = false;
		return false;
	}

	if (OutReport.UnsupportedNodeClasses.Num() > 0)
	{
		OutReport.Warnings.Add(FString::Printf(TEXT("Unsupported nodes skipped: %d"), OutReport.UnsupportedNodeClasses.Num()));
	}

	OutReport.bSuccess = OutReport.Errors.Num() == 0 && OutReport.UnsupportedNodeClasses.Num() == 0;
	return true;
}

static bool UEIRInterchange_RemoveExistingFunctionGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!IsValid(Blueprint) || GraphName.IsEmpty())
	{
		return false;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (IsValid(Graph) && Graph->GetName() == GraphName)
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			return true;
		}
	}
	return true;
}

static bool UEIRInterchange_CreateFunctionGraph(UBlueprint* Blueprint, const FString& GraphName, UEdGraph*& OutGraph)
{
	OutGraph = nullptr;
	if (!IsValid(Blueprint) || GraphName.IsEmpty())
	{
		return false;
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(*GraphName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!IsValid(NewGraph))
	{
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, false, static_cast<UFunction*>(nullptr));
	OutGraph = NewGraph;
	return true;
}

static bool UEIRInterchange_PinTypeFromIR(const FUEIRGraphPinType& PinTypeIR, FEdGraphPinType& OutPinType)
{
	OutPinType.ResetToDefaults();

	if (PinTypeIR.Category.IsEmpty())
	{
		return false;
	}

	OutPinType.PinCategory = FName(*PinTypeIR.Category);
	OutPinType.PinSubCategory = FName(*PinTypeIR.SubCategory);
	OutPinType.bIsReference = PinTypeIR.bIsReference;
	OutPinType.bIsConst = PinTypeIR.bIsConst;

	if (PinTypeIR.Container.Equals(TEXT("Array"), ESearchCase::IgnoreCase))
	{
		OutPinType.ContainerType = EPinContainerType::Array;
	}
	else if (PinTypeIR.Container.Equals(TEXT("Set"), ESearchCase::IgnoreCase))
	{
		OutPinType.ContainerType = EPinContainerType::Set;
	}
	else if (PinTypeIR.Container.Equals(TEXT("Map"), ESearchCase::IgnoreCase))
	{
		OutPinType.ContainerType = EPinContainerType::Map;
	}
	else
	{
		OutPinType.ContainerType = EPinContainerType::None;
	}

	if (!PinTypeIR.SubCategoryObjectPath.IsEmpty())
	{
		UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *PinTypeIR.SubCategoryObjectPath);
		if (IsValid(Obj))
		{
			OutPinType.PinSubCategoryObject = Obj;
		}
	}

	return true;
}

static bool UEIRInterchange_TryGetVariableTypeFromNodeIR(const FUEIRGraphNode& NodeIR, FEdGraphPinType& OutPinType)
{
	for (const FUEIRGraphPin& P : NodeIR.Pins)
	{
		if (P.Direction.Equals(TEXT("Output"), ESearchCase::IgnoreCase) && !P.Type.Category.Equals(UEdGraphSchema_K2::PC_Exec.ToString(), ESearchCase::IgnoreCase))
		{
			return UEIRInterchange_PinTypeFromIR(P.Type, OutPinType);
		}
	}
	return false;
}

static bool UEIRInterchange_EnsureMemberVariableExists(UBlueprint* Blueprint, const FString& VariableName, const FUEIRGraphNode& NodeIR)
{
	if (!IsValid(Blueprint) || VariableName.IsEmpty())
	{
		return false;
	}

	const FName VarName(*VariableName);
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == VarName)
		{
			return true;
		}
	}

	FEdGraphPinType PinType;
	if (!UEIRInterchange_TryGetVariableTypeFromNodeIR(NodeIR, PinType))
	{
		return false;
	}

	return FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinType);
}

static bool UEIRInterchange_ImportNodeIntoGraph(const FUEIRGraphNode& NodeIR, UEdGraph* Graph, UEdGraphNode*& OutNode, FString& OutError)
{
	OutNode = nullptr;
	OutError.Reset();

	if (!IsValid(Graph))
	{
		OutError = TEXT("Graph invalid");
		return false;
	}

	const FString ClassName = UEIRInterchange_NodeClassNameFromPath(NodeIR.ClassPath);
	if (!UEIRInterchange_IsSupportedK2NodeClassName(ClassName))
	{
		OutError = FString::Printf(TEXT("Unsupported node class: %s"), *NodeIR.ClassPath);
		return false;
	}

	if (ClassName == TEXT("K2Node_FunctionEntry") || ClassName == TEXT("K2Node_FunctionResult") || ClassName == TEXT("K2Node_ReturnNode"))
	{
		for (UEdGraphNode* Existing : Graph->Nodes)
		{
			if (!IsValid(Existing))
			{
				continue;
			}
			if (ClassName == TEXT("K2Node_FunctionEntry") && UEIRInterchange_IsEntryNode(Existing))
			{
				OutNode = Existing;
				OutNode->NodePosX = NodeIR.PosX;
				OutNode->NodePosY = NodeIR.PosY;
				return true;
			}
			if ((ClassName == TEXT("K2Node_FunctionResult") || ClassName == TEXT("K2Node_ReturnNode")) && UEIRInterchange_IsReturnNode(Existing))
			{
				OutNode = Existing;
				OutNode->NodePosX = NodeIR.PosX;
				OutNode->NodePosY = NodeIR.PosY;
				return true;
			}
		}

		OutError = FString::Printf(TEXT("Expected entry/return node not found in created function graph: %s"), *ClassName);
		return false;
	}

	UEdGraphNode* NewNode = nullptr;

	if (ClassName == TEXT("K2Node_IfThenElse"))
	{
		UK2Node_IfThenElse* IfNode = NewObject<UK2Node_IfThenElse>(Graph);
		NewNode = IfNode;
	}
	else if (ClassName == TEXT("K2Node_CallFunction"))
	{
		UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph);
		UFunction* Func = NodeIR.CallFunctionPath.IsEmpty() ? nullptr : Cast<UFunction>(StaticLoadObject(UFunction::StaticClass(), nullptr, *NodeIR.CallFunctionPath));
		if (!IsValid(Func))
		{
			OutError = FString::Printf(TEXT("CallFunction target function not found: %s"), *NodeIR.CallFunctionPath);
			return false;
		}
		Call->SetFromFunction(Func);
		NewNode = Call;
	}
	else if (ClassName == TEXT("K2Node_VariableGet"))
	{
		UK2Node_VariableGet* Get = NewObject<UK2Node_VariableGet>(Graph);
		Get->VariableReference.SetSelfMember(FName(*NodeIR.VariableName));
		NewNode = Get;
	}
	else if (ClassName == TEXT("K2Node_VariableSet"))
	{
		UK2Node_VariableSet* Set = NewObject<UK2Node_VariableSet>(Graph);
		Set->VariableReference.SetSelfMember(FName(*NodeIR.VariableName));
		NewNode = Set;
	}

	if (!IsValid(NewNode))
	{
		OutError = FString::Printf(TEXT("Failed to instantiate node: %s"), *NodeIR.ClassPath);
		return false;
	}

	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = NodeIR.PosX;
	NewNode->NodePosY = NodeIR.PosY;
	Graph->AddNode(NewNode, true, false);

	OutNode = NewNode;
	return true;
}

bool UUEIRInterchangeBPLibrary::ImportUEIRBlueprintFunctionGraphToBlueprint(UBlueprint* TargetBlueprint, const FUEIRBlueprintFunctionGraph& Graph, bool bReplaceIfExists, FUEIRBlueprintGraphConversionReport& OutReport)
{
	OutReport = UEIRInterchange_MakeEmptyReport();

	if (!IsValid(TargetBlueprint))
	{
		OutReport.Errors.Add(TEXT("TargetBlueprint invalid"));
		return false;
	}

	if (Graph.GraphName.IsEmpty())
	{
		OutReport.Errors.Add(TEXT("GraphName empty"));
		return false;
	}

	if (bReplaceIfExists)
	{
		if (!UEIRInterchange_RemoveExistingFunctionGraph(TargetBlueprint, Graph.GraphName))
		{
			OutReport.Errors.Add(TEXT("Failed to remove existing function graph"));
			return false;
		}
	}

	UEdGraph* NewGraph = nullptr;
	if (!UEIRInterchange_CreateFunctionGraph(TargetBlueprint, Graph.GraphName, NewGraph))
	{
		OutReport.Errors.Add(TEXT("Failed to create function graph"));
		return false;
	}

	TMap<FString, UEdGraphNode*> ImportedNodeMap;

	for (const FUEIRGraphNode& NodeIR : Graph.Nodes)
	{
		const FString ClassName = UEIRInterchange_NodeClassNameFromPath(NodeIR.ClassPath);
		if (!UEIRInterchange_IsSupportedK2NodeClassName(ClassName))
		{
			UEIRInterchange_AddUnique(OutReport.UnsupportedNodeClasses, NodeIR.ClassPath);
			continue;
		}

		if ((ClassName == TEXT("K2Node_VariableGet") || ClassName == TEXT("K2Node_VariableSet")) && !NodeIR.VariableName.IsEmpty())
		{
			if (!UEIRInterchange_EnsureMemberVariableExists(TargetBlueprint, NodeIR.VariableName, NodeIR))
			{
				OutReport.Warnings.Add(FString::Printf(TEXT("Failed to ensure member variable exists: %s"), *NodeIR.VariableName));
			}
		}

		UEdGraphNode* ImportedNode = nullptr;
		FString Error;
		if (!UEIRInterchange_ImportNodeIntoGraph(NodeIR, NewGraph, ImportedNode, Error))
		{
			OutReport.Errors.Add(Error);
			continue;
		}

		if (!ImportedNodeMap.Contains(NodeIR.Id))
		{
			ImportedNodeMap.Add(NodeIR.Id, ImportedNode);
		}

		for (const FUEIRGraphPin& PinIR : NodeIR.Pins)
		{
			const EEdGraphPinDirection Dir = UEIRInterchange_PinDirectionFromString(PinIR.Direction);
			UEdGraphPin* Pin = UEIRInterchange_FindPinByNameAndDirection(ImportedNode, PinIR.Name, Dir);
			if (Pin)
			{
				UEIRInterchange_ApplyPinDefaults(Pin, PinIR);
			}
		}
	}

	const UEdGraphSchema* Schema = NewGraph->GetSchema();
	if (!Schema)
	{
		OutReport.Errors.Add(TEXT("Graph schema missing"));
		return false;
	}

	for (const FUEIRGraphEdge& Edge : Graph.Edges)
	{
		UEdGraphNode** FromNodePtr = ImportedNodeMap.Find(Edge.From.NodeId);
		UEdGraphNode** ToNodePtr = ImportedNodeMap.Find(Edge.To.NodeId);
		if (!FromNodePtr || !ToNodePtr)
		{
			continue;
		}

		UEdGraphNode* FromNode = *FromNodePtr;
		UEdGraphNode* ToNode = *ToNodePtr;
		UEdGraphPin* FromPin = UEIRInterchange_FindPinByNameAndDirection(FromNode, Edge.From.PinName, EGPD_Output);
		UEdGraphPin* ToPin = UEIRInterchange_FindPinByNameAndDirection(ToNode, Edge.To.PinName, EGPD_Input);
		if (!FromPin || !ToPin)
		{
			continue;
		}

		Schema->TryCreateConnection(FromPin, ToPin);
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(TargetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(TargetBlueprint);

	OutReport.bSuccess = OutReport.Errors.Num() == 0 && OutReport.UnsupportedNodeClasses.Num() == 0;
	if (OutReport.UnsupportedNodeClasses.Num() > 0)
	{
		OutReport.Warnings.Add(FString::Printf(TEXT("Unsupported nodes skipped: %d"), OutReport.UnsupportedNodeClasses.Num()));
	}
	return OutReport.bSuccess;
}

bool UUEIRInterchangeBPLibrary::CreateBlueprintAndImportUEIRBlueprintFunctionGraph(const FString& PackagePath, const FString& BlueprintName, const FString& ParentClassPath, const FUEIRBlueprintFunctionGraph& Graph, UBlueprint*& OutBlueprint, FString& OutObjectPath, FUEIRBlueprintGraphConversionReport& OutReport)
{
	OutBlueprint = nullptr;
	OutObjectPath.Reset();
	OutReport = UEIRInterchange_MakeEmptyReport();

	if (PackagePath.IsEmpty() || BlueprintName.IsEmpty() || ParentClassPath.IsEmpty())
	{
		OutReport.Errors.Add(TEXT("PackagePath/BlueprintName/ParentClassPath invalid"));
		return false;
	}

	UClass* ParentClass = LoadObject<UClass>(nullptr, *ParentClassPath);
	if (!IsValid(ParentClass))
	{
		OutReport.Errors.Add(FString::Printf(TEXT("Parent class not found: %s"), *ParentClassPath));
		return false;
	}

	const FString LongPackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *BlueprintName);
	UPackage* Package = CreatePackage(*LongPackageName);
	if (!IsValid(Package))
	{
		OutReport.Errors.Add(TEXT("CreatePackage failed"));
		return false;
	}

	UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*BlueprintName), EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
	if (!IsValid(NewBP))
	{
		OutReport.Errors.Add(TEXT("CreateBlueprint failed"));
		return false;
	}

	FAssetRegistryModule::AssetCreated(NewBP);
	NewBP->MarkPackageDirty();

	FUEIRBlueprintGraphConversionReport ImportReport;
	const bool bImported = ImportUEIRBlueprintFunctionGraphToBlueprint(NewBP, Graph, true, ImportReport);
	OutReport = ImportReport;

	OutBlueprint = NewBP;
	OutObjectPath = NewBP->GetPathName();
	return bImported;
}

FString UUEIRInterchangeBPLibrary::BlueprintGraphReportToMarkdown(const FUEIRBlueprintGraphConversionReport& Report)
{
	FString Out;
	Out += TEXT("# UEIR BlueprintGraph Conversion Report\n\n");
	Out += FString::Printf(TEXT("- Success: %s\n"), Report.bSuccess ? TEXT("Yes") : TEXT("No"));
	Out += TEXT("\n");

	if (Report.Errors.Num() > 0)
	{
		Out += TEXT("## Errors\n\n");
		for (const FString& E : Report.Errors)
		{
			Out += FString::Printf(TEXT("- %s\n"), *E);
		}
		Out += TEXT("\n");
	}

	if (Report.Warnings.Num() > 0)
	{
		Out += TEXT("## Warnings\n\n");
		for (const FString& W : Report.Warnings)
		{
			Out += FString::Printf(TEXT("- %s\n"), *W);
		}
		Out += TEXT("\n");
	}

	if (Report.UnsupportedNodeClasses.Num() > 0)
	{
		Out += TEXT("## Unsupported Nodes\n\n");
		for (const FString& C : Report.UnsupportedNodeClasses)
		{
			Out += FString::Printf(TEXT("- %s\n"), *C);
		}
		Out += TEXT("\n");
	}

	return Out;
}

static FString UEIRInterchange_GetDefaultUEIRGeneratedOutputDir()
{
	return FPaths::Combine(UUEIRInterchangeBPLibrary::GetDefaultUEIROutputDir(), TEXT("Generated"));
}

static FString UEIRInterchange_SanitizeIdentifier(const FString& In)
{
	FString Out;
	Out.Reserve(In.Len());
	for (int32 i = 0; i < In.Len(); ++i)
	{
		const TCHAR C = In[i];
		const bool bOk =
			(C >= TEXT('A') && C <= TEXT('Z')) ||
			(C >= TEXT('a') && C <= TEXT('z')) ||
			(C >= TEXT('0') && C <= TEXT('9')) ||
			(C == TEXT('_'));
		Out.AppendChar(bOk ? C : TEXT('_'));
	}
	if (Out.IsEmpty())
	{
		Out = TEXT("UEIR_Generated");
	}
	if (Out.Len() > 0 && Out[0] >= TEXT('0') && Out[0] <= TEXT('9'))
	{
		Out = TEXT("_") + Out;
	}
	return Out;
}

static bool UEIRInterchange_IsExecPinIR(const FUEIRGraphNode& Node, const FString& PinName, const FString& Direction)
{
	for (const FUEIRGraphPin& P : Node.Pins)
	{
		if (P.Name == PinName && P.Direction.Equals(Direction, ESearchCase::IgnoreCase))
		{
			return P.Type.Category.Equals(UEdGraphSchema_K2::PC_Exec.ToString(), ESearchCase::IgnoreCase);
		}
	}
	return false;
}

static const FUEIRGraphPin* UEIRInterchange_FindPinIR(const FUEIRGraphNode& Node, const FString& PinName, const FString& Direction)
{
	for (const FUEIRGraphPin& P : Node.Pins)
	{
		if (P.Name == PinName && P.Direction.Equals(Direction, ESearchCase::IgnoreCase))
		{
			return &P;
		}
	}
	return nullptr;
}

static const FUEIRGraphPin* UEIRInterchange_FindFirstDataPinIR(const FUEIRGraphNode& Node, const FString& Direction)
{
	for (const FUEIRGraphPin& P : Node.Pins)
	{
		if (P.Direction.Equals(Direction, ESearchCase::IgnoreCase) && !P.Type.Category.Equals(UEdGraphSchema_K2::PC_Exec.ToString(), ESearchCase::IgnoreCase))
		{
			return &P;
		}
	}
	return nullptr;
}

static FString UEIRInterchange_CppTypeFromPinType(const FUEIRGraphPinType& T, bool& bOutOk)
{
	bOutOk = true;
	const FString Cat = T.Category.ToLower();
	if (Cat == TEXT("bool"))
	{
		return TEXT("bool");
	}
	if (Cat == TEXT("int"))
	{
		return TEXT("int32");
	}
	if (Cat == TEXT("int64"))
	{
		return TEXT("int64");
	}
	if (Cat == TEXT("float"))
	{
		return TEXT("float");
	}
	if (Cat == TEXT("double"))
	{
		return TEXT("double");
	}
	if (Cat == TEXT("string"))
	{
		return TEXT("FString");
	}
	if (Cat == TEXT("name"))
	{
		return TEXT("FName");
	}
	if (Cat == TEXT("text"))
	{
		return TEXT("FText");
	}
	if (Cat == TEXT("object") || Cat == TEXT("class") || Cat == TEXT("interface"))
	{
		return TEXT("UObject*");
	}

	bOutOk = false;
	return TEXT("void");
}

static FString UEIRInterchange_CppLiteralFromPinDefault(const FUEIRGraphPin& Pin)
{
	const FString Cat = Pin.Type.Category.ToLower();
	if (Cat == TEXT("bool"))
	{
		if (Pin.DefaultValue.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			return TEXT("true");
		}
		return TEXT("false");
	}
	if (Cat == TEXT("int") || Cat == TEXT("int64") || Cat == TEXT("float") || Cat == TEXT("double"))
	{
		if (!Pin.DefaultValue.IsEmpty())
		{
			return Pin.DefaultValue;
		}
		return TEXT("0");
	}
	if (Cat == TEXT("string"))
	{
		return FString::Printf(TEXT("TEXT(\"%s\")"), *Pin.DefaultValue.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")));
	}
	if (Cat == TEXT("name"))
	{
		return FString::Printf(TEXT("FName(TEXT(\"%s\"))"), *Pin.DefaultValue.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")));
	}
	if (Cat == TEXT("text"))
	{
		return FString::Printf(TEXT("FText::FromString(TEXT(\"%s\"))"), *Pin.DefaultValue.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")));
	}
	if (Cat == TEXT("object") || Cat == TEXT("class") || Cat == TEXT("interface"))
	{
		if (!Pin.DefaultObjectPath.IsEmpty())
		{
			return FString::Printf(TEXT("LoadObject<UObject>(nullptr, TEXT(\"%s\"))"), *Pin.DefaultObjectPath.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")));
		}
		return TEXT("nullptr");
	}
	return TEXT("{}");
}

struct FUEIRInterchange_CppEmitter
{
	TArray<FString> Lines;
	int32 Indent = 0;

	void AddRaw(const FString& L)
	{
		Lines.Add(L);
	}

	void Add(const FString& L)
	{
		Lines.Add(FString::ChrN(Indent, TEXT('\t')) + L);
	}

	void Push()
	{
		++Indent;
	}

	void Pop()
	{
		Indent = FMath::Max(0, Indent - 1);
	}

	FString ToString() const
	{
		return FString::Join(Lines, TEXT("\n")) + TEXT("\n");
	}
};

static bool UEIRInterchange_TryGetUFunctionCppType(const FProperty* Prop, FString& OutCppType, TSet<FString>& OutForwardDecls)
{
	OutCppType.Reset();
	if (!Prop)
	{
		return false;
	}

	if (Prop->IsA<FBoolProperty>())
	{
		OutCppType = TEXT("bool");
		return true;
	}
	if (Prop->IsA<FIntProperty>())
	{
		OutCppType = TEXT("int32");
		return true;
	}
	if (Prop->IsA<FInt64Property>())
	{
		OutCppType = TEXT("int64");
		return true;
	}
	if (Prop->IsA<FFloatProperty>())
	{
		OutCppType = TEXT("float");
		return true;
	}
	if (Prop->IsA<FDoubleProperty>())
	{
		OutCppType = TEXT("double");
		return true;
	}
	if (Prop->IsA<FStrProperty>())
	{
		OutCppType = TEXT("FString");
		return true;
	}
	if (Prop->IsA<FNameProperty>())
	{
		OutCppType = TEXT("FName");
		return true;
	}
	if (Prop->IsA<FTextProperty>())
	{
		OutCppType = TEXT("FText");
		return true;
	}
	if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
	{
		if (UClass* Cls = ObjProp->PropertyClass)
		{
			const FString ClsName = Cls->GetName();
			OutForwardDecls.Add(ClsName);
			OutCppType = FString::Printf(TEXT("%s*"), *ClsName);
			return true;
		}
		OutCppType = TEXT("UObject*");
		return true;
	}
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		if (UScriptStruct* S = StructProp->Struct)
		{
			OutCppType = S->GetName();
			return true;
		}
	}

	return false;
}

static const FUEIRGraphNode* UEIRInterchange_FindNodeByClassName(const FUEIRBlueprintFunctionGraph& Graph, const FString& ClassName)
{
	for (const FUEIRGraphNode& N : Graph.Nodes)
	{
		if (UEIRInterchange_NodeClassNameFromPath(N.ClassPath) == ClassName)
		{
			return &N;
		}
	}
	return nullptr;
}

static void UEIRInterchange_BuildEdgeMaps(
	const FUEIRBlueprintFunctionGraph& Graph,
	const TMap<FString, const FUEIRGraphNode*>& NodeMap,
	TMap<FString, FUEIRGraphPinRef>& OutDataSources,
	TMultiMap<FString, FUEIRGraphPinRef>& OutExecNext)
{
	OutDataSources.Reset();
	OutExecNext.Reset();

	for (const FUEIRGraphEdge& E : Graph.Edges)
	{
		const FUEIRGraphNode* FromNode = NodeMap.FindRef(E.From.NodeId);
		const FUEIRGraphNode* ToNode = NodeMap.FindRef(E.To.NodeId);
		if (!FromNode || !ToNode)
		{
			continue;
		}

		const bool bExec =
			UEIRInterchange_IsExecPinIR(*FromNode, E.From.PinName, TEXT("Output")) ||
			UEIRInterchange_IsExecPinIR(*ToNode, E.To.PinName, TEXT("Input"));

		const FString Key = FString::Printf(TEXT("%s|%s"), *E.To.NodeId, *E.To.PinName);
		if (bExec)
		{
			const FString ExecKey = FString::Printf(TEXT("%s|%s"), *E.From.NodeId, *E.From.PinName);
			OutExecNext.Add(ExecKey, E.To);
		}
		else
		{
			if (!OutDataSources.Contains(Key))
			{
				OutDataSources.Add(Key, E.From);
			}
		}
	}
}

static FString UEIRInterchange_GetExecNextNodeId(const TMultiMap<FString, FUEIRGraphPinRef>& ExecNext, const FString& FromNodeId, const FString& FromPinName)
{
	const FString Key = FString::Printf(TEXT("%s|%s"), *FromNodeId, *FromPinName);
	TArray<FUEIRGraphPinRef> Values;
	ExecNext.MultiFind(Key, Values);
	return Values.Num() > 0 ? Values[0].NodeId : FString();
}

static bool UEIRInterchange_TryResolveDataSource(const TMap<FString, FUEIRGraphPinRef>& DataSources, const FString& ToNodeId, const FString& ToPinName, FUEIRGraphPinRef& OutFrom)
{
	const FString Key = FString::Printf(TEXT("%s|%s"), *ToNodeId, *ToPinName);
	if (const FUEIRGraphPinRef* Found = DataSources.Find(Key))
	{
		OutFrom = *Found;
		return true;
	}
	return false;
}

static bool UEIRInterchange_IsSupportedNodeClassForCpp(const FString& ClassName)
{
	return
		ClassName == TEXT("K2Node_FunctionEntry") ||
		ClassName == TEXT("K2Node_FunctionResult") ||
		ClassName == TEXT("K2Node_IfThenElse") ||
		ClassName == TEXT("K2Node_CallFunction") ||
		ClassName == TEXT("K2Node_VariableGet") ||
		ClassName == TEXT("K2Node_VariableSet");
}

static bool UEIRInterchange_LoadUFunctionForCall(const FString& FunctionPath, UFunction*& OutFunc)
{
	OutFunc = nullptr;
	if (FunctionPath.IsEmpty())
	{
		return false;
	}
	OutFunc = Cast<UFunction>(StaticLoadObject(UFunction::StaticClass(), nullptr, *FunctionPath));
	return IsValid(OutFunc);
}

static bool UEIRInterchange_IsCallFunctionNodeSupportedForCpp(const FUEIRGraphNode& NodeIR, FString& OutReason)
{
	OutReason.Reset();
	if (UEIRInterchange_NodeClassNameFromPath(NodeIR.ClassPath) != TEXT("K2Node_CallFunction"))
	{
		return true;
	}

	UFunction* Func = nullptr;
	if (!UEIRInterchange_LoadUFunctionForCall(NodeIR.CallFunctionPath, Func))
	{
		OutReason = FString::Printf(TEXT("CallFunction target not found: %s"), *NodeIR.CallFunctionPath);
		return false;
	}

	const bool bOk = Func->HasAnyFunctionFlags(FUNC_Static) || Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
	if (!bOk)
	{
		OutReason = FString::Printf(TEXT("CallFunction not BlueprintPure or Static: %s"), *NodeIR.CallFunctionPath);
		return false;
	}
	return true;
}

static bool UEIRInterchange_WriteTextFileEnsureDir(const FString& FilePath, const FString& Text)
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(FilePath);
	const FString Directory = FPaths::GetPath(FullPath);
	if (!Directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}
	return FFileHelper::SaveStringToFile(Text, *FullPath);
}

static FString UEIRInterchange_GenFunctionSignatureFromGraph(
	const FString& PublicName,
	const FUEIRBlueprintFunctionGraph& Graph,
	TMap<FString, FString>& OutEntryParamNameMap,
	FString& OutReturnCppType,
	bool& bOutHasReturnValue,
	bool& bOutOk)
{
	bOutOk = true;
	bOutHasReturnValue = false;
	OutEntryParamNameMap.Reset();

	const FUEIRGraphNode* Entry = UEIRInterchange_FindNodeByClassName(Graph, TEXT("K2Node_FunctionEntry"));
	const FUEIRGraphNode* Ret = UEIRInterchange_FindNodeByClassName(Graph, TEXT("K2Node_FunctionResult"));
	if (!Entry || !Ret)
	{
		bOutOk = false;
		return FString();
	}

	TArray<FString> Params;
	Params.Add(TEXT("UObject* Context"));

	for (const FUEIRGraphPin& P : Entry->Pins)
	{
		if (!P.Direction.Equals(TEXT("Output"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (P.Type.Category.Equals(UEdGraphSchema_K2::PC_Exec.ToString(), ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (P.Name.Equals(TEXT("self"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		bool bTypeOk = false;
		const FString CppType = UEIRInterchange_CppTypeFromPinType(P.Type, bTypeOk);
		if (!bTypeOk)
		{
			bOutOk = false;
			return FString();
		}

		const FString ParamName = UEIRInterchange_SanitizeIdentifier(P.Name);
		OutEntryParamNameMap.Add(P.Name, ParamName);
		Params.Add(FString::Printf(TEXT("%s %s"), *CppType, *ParamName));
	}

	TArray<const FUEIRGraphPin*> RetValuePins;
	for (const FUEIRGraphPin& P : Ret->Pins)
	{
		if (!P.Direction.Equals(TEXT("Input"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (P.Type.Category.Equals(UEdGraphSchema_K2::PC_Exec.ToString(), ESearchCase::IgnoreCase))
		{
			continue;
		}
		RetValuePins.Add(&P);
	}

	if (RetValuePins.Num() == 0)
	{
		OutReturnCppType = TEXT("void");
		bOutHasReturnValue = false;
	}
	else if (RetValuePins.Num() == 1)
	{
		bool bTypeOk = false;
		OutReturnCppType = UEIRInterchange_CppTypeFromPinType(RetValuePins[0]->Type, bTypeOk);
		if (!bTypeOk)
		{
			bOutOk = false;
			return FString();
		}
		bOutHasReturnValue = true;
	}
	else
	{
		bOutOk = false;
		return FString();
	}

	return FString::Printf(TEXT("%s %s(%s)"), *OutReturnCppType, *PublicName, *FString::Join(Params, TEXT(", ")));
}

static bool UEIRInterchange_GenExprForPinRef(
	const FUEIRGraphPinRef& From,
	const FUEIRBlueprintFunctionGraph& Graph,
	const TMap<FString, const FUEIRGraphNode*>& NodeMap,
	const TMap<FString, FString>& EntryParamNameMap,
	const TMap<FString, FString>& LocalVarNameMap,
	const TMap<FString, FUEIRGraphPinRef>& DataSources,
	FUEIRInterchange_CppEmitter& Emit,
	TSet<FString>& InOutForwardDecls,
	TMap<FString, FString>& InOutPureCallTempByOutputPin,
	int32& InOutCallIndex,
	FString& OutExpr,
	FString& OutError)
{
	OutExpr.Reset();
	OutError.Reset();

	const FUEIRGraphNode* Node = NodeMap.FindRef(From.NodeId);
	if (!Node)
	{
		OutError = TEXT("Expr source node not found");
		return false;
	}

	const FString ClassName = UEIRInterchange_NodeClassNameFromPath(Node->ClassPath);
	if (ClassName == TEXT("K2Node_FunctionEntry"))
	{
		if (const FString* Param = EntryParamNameMap.Find(From.PinName))
		{
			OutExpr = *Param;
			return true;
		}
		OutError = FString::Printf(TEXT("Entry pin not mapped: %s"), *From.PinName);
		return false;
	}

	if (ClassName == TEXT("K2Node_VariableGet"))
	{
		if (const FString* Local = LocalVarNameMap.Find(Node->VariableName))
		{
			OutExpr = *Local;
			return true;
		}
		OutError = FString::Printf(TEXT("Variable not mapped: %s"), *Node->VariableName);
		return false;
	}

	if (ClassName == TEXT("K2Node_CallFunction"))
	{
		const FUEIRGraphPin* OutDataPin = UEIRInterchange_FindFirstDataPinIR(*Node, TEXT("Output"));
		if (!OutDataPin)
		{
			OutError = TEXT("Pure CallFunction output pin missing");
			return false;
		}

		const FString OutputKey = FString::Printf(TEXT("%s|%s"), *Node->Id, *OutDataPin->Name);
		if (const FString* TempVar = InOutPureCallTempByOutputPin.Find(OutputKey))
		{
			OutExpr = *TempVar;
			return true;
		}

		UFunction* Func = nullptr;
		if (!UEIRInterchange_LoadUFunctionForCall(Node->CallFunctionPath, Func))
		{
			OutError = FString::Printf(TEXT("CallFunction target not found: %s"), *Node->CallFunctionPath);
			return false;
		}

		TSet<FString> LocalForwardDecls;
		FString RetType;
		bool bHasReturn = false;
		for (TFieldIterator<FProperty> It(Func); It; ++It)
		{
			const FProperty* Prop = *It;
			if (!Prop || !Prop->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}
			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				bHasReturn = UEIRInterchange_TryGetUFunctionCppType(Prop, RetType, LocalForwardDecls);
				break;
			}
		}
		if (!bHasReturn)
		{
			OutError = TEXT("Pure CallFunction return type unsupported");
			return false;
		}

		for (const FString& Fwd : LocalForwardDecls)
		{
			InOutForwardDecls.Add(Fwd);
		}

		const FString TempName = FString::Printf(TEXT("UEIR_Temp_%d"), InOutCallIndex++);
		InOutPureCallTempByOutputPin.Add(OutputKey, TempName);
		Emit.Add(FString::Printf(TEXT("%s %s{};"), *RetType, *TempName));

		const FString FuncVar = FString::Printf(TEXT("UEIR_Func_%d"), InOutCallIndex);
		const FString ParamsVar = FString::Printf(TEXT("UEIR_Params_%d"), InOutCallIndex);
		const FString ParamsType = FString::Printf(TEXT("FUEIR_Params_%d"), InOutCallIndex);

		Emit.Add(FString::Printf(TEXT("UFunction* %s = Cast<UFunction>(StaticLoadObject(UFunction::StaticClass(), nullptr, TEXT(\"%s\")));"), *FuncVar, *Node->CallFunctionPath.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""))));
		Emit.Add(FString::Printf(TEXT("if (%s)"), *FuncVar));
		Emit.Add(TEXT("{"));
		Emit.Push();

		Emit.Add(FString::Printf(TEXT("struct %s"), *ParamsType));
		Emit.Add(TEXT("{"));
		Emit.Push();
		for (TFieldIterator<FProperty> It(Func); It; ++It)
		{
			const FProperty* Prop = *It;
			if (!Prop || !Prop->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}
			FString CppType;
			TSet<FString> Dummy;
			if (!UEIRInterchange_TryGetUFunctionCppType(Prop, CppType, Dummy))
			{
				OutError = TEXT("CallFunction parameter type unsupported");
				return false;
			}
			const FString MemberName = UEIRInterchange_SanitizeIdentifier(Prop->GetName());
			Emit.Add(FString::Printf(TEXT("%s %s{};"), *CppType, *MemberName));
		}
		Emit.Pop();
		Emit.Add(TEXT("};"));
		Emit.Add(FString::Printf(TEXT("%s %s;"), *ParamsType, *ParamsVar));

		for (TFieldIterator<FProperty> It(Func); It; ++It)
		{
			const FProperty* Prop = *It;
			if (!Prop || !Prop->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}
			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm) || Prop->HasAnyPropertyFlags(CPF_OutParm))
			{
				continue;
			}

			const FString ParamPinName = Prop->GetName();
			const FUEIRGraphPin* NodePin = UEIRInterchange_FindPinIR(*Node, ParamPinName, TEXT("Input"));
			if (!NodePin)
			{
				continue;
			}

			FUEIRGraphPinRef Src;
			FString ValueExpr;
			if (UEIRInterchange_TryResolveDataSource(DataSources, Node->Id, NodePin->Name, Src))
			{
				FString Err;
				FString DummyType;
				if (!UEIRInterchange_GenExprForPinRef(Src, Graph, NodeMap, EntryParamNameMap, LocalVarNameMap, DataSources, Emit, InOutForwardDecls, InOutPureCallTempByOutputPin, InOutCallIndex, ValueExpr, Err))
				{
					OutError = Err;
					return false;
				}
			}
			else
			{
				ValueExpr = UEIRInterchange_CppLiteralFromPinDefault(*NodePin);
			}

			const FString MemberName = UEIRInterchange_SanitizeIdentifier(Prop->GetName());
			Emit.Add(FString::Printf(TEXT("%s.%s = %s;"), *ParamsVar, *MemberName, *ValueExpr));
		}

		Emit.Add(FString::Printf(TEXT("UObject* UEIR_Target = %s->HasAnyFunctionFlags(FUNC_Static) ? %s->GetOuterUClass()->GetDefaultObject() : Context;"), *FuncVar, *FuncVar));
		Emit.Add(TEXT("if (UEIR_Target)"));
		Emit.Add(TEXT("{"));
		Emit.Push();
		Emit.Add(FString::Printf(TEXT("UEIR_Target->ProcessEvent(%s, &%s);"), *FuncVar, *ParamsVar));
		Emit.Pop();
		Emit.Add(TEXT("}"));

		Emit.Add(FString::Printf(TEXT("%s = %s.ReturnValue;"), *TempName, *ParamsVar));

		Emit.Pop();
		Emit.Add(TEXT("}"));

		OutExpr = TempName;
		return true;
	}

	OutError = FString::Printf(TEXT("Expr source unsupported class: %s"), *ClassName);
	return false;
}

static bool UEIRInterchange_GenerateCppFromGraphIR(
	const FString& BlueprintName,
	const FString& FunctionGraphName,
	const FUEIRBlueprintFunctionGraph& Graph,
	const FString& OutputDir,
	FString& OutHeaderFile,
	FString& OutCppFile,
	FString& OutReportMarkdown)
{
	OutHeaderFile.Reset();
	OutCppFile.Reset();
	OutReportMarkdown.Reset();

	bool bDegraded = false;
	TArray<FString> Reasons;

	TMap<FString, const FUEIRGraphNode*> NodeMap;
	NodeMap.Reserve(Graph.Nodes.Num());
	for (const FUEIRGraphNode& N : Graph.Nodes)
	{
		NodeMap.Add(N.Id, &N);
		const FString ClassName = UEIRInterchange_NodeClassNameFromPath(N.ClassPath);
		if (!UEIRInterchange_IsSupportedNodeClassForCpp(ClassName))
		{
			bDegraded = true;
			Reasons.Add(FString::Printf(TEXT("Unsupported node class: %s"), *N.ClassPath));
		}
		if (!bDegraded && ClassName == TEXT("K2Node_CallFunction"))
		{
			FString Reason;
			if (!UEIRInterchange_IsCallFunctionNodeSupportedForCpp(N, Reason))
			{
				bDegraded = true;
				Reasons.Add(Reason);
			}
		}
	}

	const FString PublicFuncName = UEIRInterchange_SanitizeIdentifier(FString::Printf(TEXT("UEIR_%s_%s"), *BlueprintName, *FunctionGraphName));

	TMap<FString, FString> EntryParamNameMap;
	FString ReturnCppType;
	bool bHasReturn = false;
	bool bSigOk = false;
	const FString Signature = UEIRInterchange_GenFunctionSignatureFromGraph(PublicFuncName, Graph, EntryParamNameMap, ReturnCppType, bHasReturn, bSigOk);
	if (!bSigOk)
	{
		bDegraded = true;
		Reasons.Add(TEXT("Function signature unsupported (params/return)"));
	}

	TMap<FString, FString> LocalVarNameMap;
	if (!bDegraded)
	{
		for (const FUEIRGraphNode& N : Graph.Nodes)
		{
			const FString ClassName = UEIRInterchange_NodeClassNameFromPath(N.ClassPath);
			if ((ClassName == TEXT("K2Node_VariableGet") || ClassName == TEXT("K2Node_VariableSet")) && !N.VariableName.IsEmpty())
			{
				if (!LocalVarNameMap.Contains(N.VariableName))
				{
					LocalVarNameMap.Add(N.VariableName, UEIRInterchange_SanitizeIdentifier(TEXT("Local_") + N.VariableName));
				}
			}
		}
	}
	if (!bDegraded)
	{
		for (const TPair<FString, FString>& Pair : LocalVarNameMap)
		{
			const FString VarName = Pair.Key;
			const FUEIRGraphNode* VarNode = nullptr;
			for (const FUEIRGraphNode& N : Graph.Nodes)
			{
				if (N.VariableName == VarName)
				{
					VarNode = &N;
					break;
				}
			}
			const FUEIRGraphPin* TypePin = VarNode ? UEIRInterchange_FindFirstDataPinIR(*VarNode, TEXT("Output")) : nullptr;
			if (!TypePin)
			{
				TypePin = VarNode ? UEIRInterchange_FindFirstDataPinIR(*VarNode, TEXT("Input")) : nullptr;
			}
			bool bTypeOk = false;
			if (!TypePin || !UEIRInterchange_CppTypeFromPinType(TypePin->Type, bTypeOk).Len() || !bTypeOk)
			{
				bDegraded = true;
				Reasons.Add(FString::Printf(TEXT("Variable type unsupported: %s"), *VarName));
				break;
			}
		}
	}

	TMap<FString, FUEIRGraphPinRef> DataSources;
	TMultiMap<FString, FUEIRGraphPinRef> ExecNext;
	UEIRInterchange_BuildEdgeMaps(Graph, NodeMap, DataSources, ExecNext);

	const FUEIRGraphNode* Entry = UEIRInterchange_FindNodeByClassName(Graph, TEXT("K2Node_FunctionEntry"));
	const FUEIRGraphNode* Ret = UEIRInterchange_FindNodeByClassName(Graph, TEXT("K2Node_FunctionResult"));

	if (!Entry || !Ret)
	{
		bDegraded = true;
		Reasons.Add(TEXT("Entry/Return node missing"));
	}

	TSet<FString> ForwardDecls;
	FUEIRInterchange_CppEmitter H;
	H.AddRaw(TEXT("#pragma once"));
	H.AddRaw(TEXT("#include \"CoreMinimal.h\""));
	H.AddRaw(TEXT(""));
	H.AddRaw(TEXT("namespace UEIRGenerated"));
	H.AddRaw(TEXT("{"));
	H.Indent = 1;
	if (!Signature.IsEmpty())
	{
		H.Add(Signature + TEXT(";"));
	}
	else
	{
		H.Add(FString::Printf(TEXT("void %s(UObject* Context);"), *PublicFuncName));
	}
	H.Indent = 0;
	H.AddRaw(TEXT("}"));

	FUEIRInterchange_CppEmitter C;
	C.AddRaw(TEXT("#include \"CoreMinimal.h\""));
	C.AddRaw(TEXT("#include \"UObject/Class.h\""));
	C.AddRaw(TEXT("#include \"UObject/UObjectGlobals.h\""));
	C.AddRaw(TEXT(""));
	C.AddRaw(TEXT("namespace UEIRGenerated"));
	C.AddRaw(TEXT("{"));
	C.Push();

	auto EmitDefaultReturn = [&]()
	{
		if (ReturnCppType == TEXT("void"))
		{
			C.Add(TEXT("return;"));
		}
		else
		{
			C.Add(TEXT("return {};"));
		}
	};

	if (bDegraded)
	{
		const FString FallbackSig = Signature.IsEmpty() ? FString::Printf(TEXT("void %s(UObject* Context)"), *PublicFuncName) : Signature;
		C.Add(FallbackSig);
		C.Add(TEXT("{"));
		C.Push();
		C.Add(TEXT("if (!IsValid(Context))"));
		C.Add(TEXT("{"));
		C.Push();
		EmitDefaultReturn();
		C.Pop();
		C.Add(TEXT("}"));
		C.Add(FString::Printf(TEXT("static const FName UEIR_FuncName(TEXT(\"%s\"));"), *FunctionGraphName.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""))));
		C.Add(TEXT("if (UFunction* UEIR_Func = Context->FindFunction(UEIR_FuncName))"));
		C.Add(TEXT("{"));
		C.Push();
		C.Add(TEXT("Context->ProcessEvent(UEIR_Func, nullptr);"));
		C.Pop();
		C.Add(TEXT("}"));
		EmitDefaultReturn();
		C.Pop();
		C.Add(TEXT("}"));
	}
	else
	{
		C.Add(Signature);
		C.Add(TEXT("{"));
		C.Push();

		for (const TPair<FString, FString>& Pair : LocalVarNameMap)
		{
			const FString VarName = Pair.Key;
			const FString LocalName = Pair.Value;
			const FUEIRGraphNode* VarNode = nullptr;
			for (const FUEIRGraphNode& N : Graph.Nodes)
			{
				if (N.VariableName == VarName)
				{
					VarNode = &N;
					break;
				}
			}
			const FUEIRGraphPin* TypePin = VarNode ? UEIRInterchange_FindFirstDataPinIR(*VarNode, TEXT("Output")) : nullptr;
			if (!TypePin)
			{
				TypePin = VarNode ? UEIRInterchange_FindFirstDataPinIR(*VarNode, TEXT("Input")) : nullptr;
			}
			bool bTypeOk = false;
			const FString CppType = TypePin ? UEIRInterchange_CppTypeFromPinType(TypePin->Type, bTypeOk) : FString();
			C.Add(FString::Printf(TEXT("%s %s{};"), *CppType, *LocalName));
		}

		const FUEIRGraphPin* EntryExec = UEIRInterchange_FindPinIR(*Entry, UEdGraphSchema_K2::PN_Then.ToString(), TEXT("Output"));
		const FString StartNodeId = EntryExec ? UEIRInterchange_GetExecNextNodeId(ExecNext, Entry->Id, EntryExec->Name) : FString();
		if (StartNodeId.IsEmpty())
		{
			C.Add(TEXT("return;"));
		}
		else
		{
			TSet<FString> Visited;
			TMap<FString, FString> PureCallTempByOutputPin;
			int32 CallIndex = 0;

			TFunction<bool(const FString&)> GenStmt;
			GenStmt = [&](const FString& NodeId) -> bool
			{
				if (NodeId.IsEmpty() || Visited.Contains(NodeId))
				{
					return false;
				}
				Visited.Add(NodeId);

				const FUEIRGraphNode* Node = NodeMap.FindRef(NodeId);
				if (!Node)
				{
					return false;
				}

				const FString ClassName = UEIRInterchange_NodeClassNameFromPath(Node->ClassPath);
				if (ClassName == TEXT("K2Node_FunctionResult"))
				{
					if (!bHasReturn)
					{
						C.Add(TEXT("return;"));
						return true;
					}

					const FUEIRGraphPin* RetPin = UEIRInterchange_FindFirstDataPinIR(*Node, TEXT("Input"));
					if (!RetPin)
					{
						C.Add(TEXT("return {};"));
						return true;
					}

					FUEIRGraphPinRef Src;
					FString Expr;
					if (UEIRInterchange_TryResolveDataSource(DataSources, Node->Id, RetPin->Name, Src))
					{
						FString Err;
						FString DummyType;
						if (!UEIRInterchange_GenExprForPinRef(Src, Graph, NodeMap, EntryParamNameMap, LocalVarNameMap, DataSources, C, ForwardDecls, PureCallTempByOutputPin, CallIndex, Expr, Err))
						{
							C.Add(TEXT("return {};"));
							return true;
						}
					}
					else
					{
						Expr = UEIRInterchange_CppLiteralFromPinDefault(*RetPin);
					}

					C.Add(FString::Printf(TEXT("return %s;"), *Expr));
					return true;
				}

				if (ClassName == TEXT("K2Node_VariableSet"))
				{
					const FString* Local = LocalVarNameMap.Find(Node->VariableName);
					if (!Local)
					{
						return false;
					}

					const FUEIRGraphPin* ValPin = UEIRInterchange_FindPinIR(*Node, TEXT("Value"), TEXT("Input"));
					if (!ValPin)
					{
						ValPin = UEIRInterchange_FindFirstDataPinIR(*Node, TEXT("Input"));
					}

					FString Expr = ValPin ? UEIRInterchange_CppLiteralFromPinDefault(*ValPin) : TEXT("{}");
					if (ValPin)
					{
						FUEIRGraphPinRef Src;
						if (UEIRInterchange_TryResolveDataSource(DataSources, Node->Id, ValPin->Name, Src))
						{
							FString Err;
							FString DummyType;
							if (!UEIRInterchange_GenExprForPinRef(Src, Graph, NodeMap, EntryParamNameMap, LocalVarNameMap, DataSources, C, ForwardDecls, PureCallTempByOutputPin, CallIndex, Expr, Err))
							{
								Expr = UEIRInterchange_CppLiteralFromPinDefault(*ValPin);
							}
						}
					}

					C.Add(FString::Printf(TEXT("%s = %s;"), **Local, *Expr));

					const FString Next = UEIRInterchange_GetExecNextNodeId(ExecNext, Node->Id, UEdGraphSchema_K2::PN_Then.ToString());
					return Next.IsEmpty() ? true : GenStmt(Next);
				}

				if (ClassName == TEXT("K2Node_CallFunction"))
				{
					UFunction* Func = nullptr;
					if (!UEIRInterchange_LoadUFunctionForCall(Node->CallFunctionPath, Func))
					{
						return false;
					}

					const FString FuncVar = FString::Printf(TEXT("UEIR_CallFunc_%d"), CallIndex++);
					const FString ParamsVar = FString::Printf(TEXT("UEIR_CallParams_%d"), CallIndex);
					const FString ParamsType = FString::Printf(TEXT("FUEIR_CallParams_%d"), CallIndex);

					TSet<FString> LocalForwardDecls;
					bool bParamsOk = true;
					TArray<const FProperty*> ParmProps;
					for (TFieldIterator<FProperty> It(Func); It; ++It)
					{
						const FProperty* Prop = *It;
						if (Prop && Prop->HasAnyPropertyFlags(CPF_Parm))
						{
							ParmProps.Add(Prop);
							FString CppType;
							if (!UEIRInterchange_TryGetUFunctionCppType(Prop, CppType, LocalForwardDecls))
							{
								bParamsOk = false;
								break;
							}
						}
					}
					if (!bParamsOk)
					{
						return false;
					}

					for (const FString& Fwd : LocalForwardDecls)
					{
						ForwardDecls.Add(Fwd);
					}

					C.Add(FString::Printf(TEXT("UFunction* %s = Cast<UFunction>(StaticLoadObject(UFunction::StaticClass(), nullptr, TEXT(\"%s\")));"), *FuncVar, *Node->CallFunctionPath.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""))));
					C.Add(FString::Printf(TEXT("if (%s)"), *FuncVar));
					C.Add(TEXT("{"));
					C.Push();

					C.Add(FString::Printf(TEXT("struct %s"), *ParamsType));
					C.Add(TEXT("{"));
					C.Push();
					for (const FProperty* Prop : ParmProps)
					{
						FString CppType;
						TSet<FString> Dummy;
						UEIRInterchange_TryGetUFunctionCppType(Prop, CppType, Dummy);
						C.Add(FString::Printf(TEXT("%s %s{};"), *CppType, *UEIRInterchange_SanitizeIdentifier(Prop->GetName())));
					}
					C.Pop();
					C.Add(TEXT("};"));
					C.Add(FString::Printf(TEXT("%s %s;"), *ParamsType, *ParamsVar));

					for (const FProperty* Prop : ParmProps)
					{
						if (!Prop || !Prop->HasAnyPropertyFlags(CPF_Parm))
						{
							continue;
						}
						if (Prop->HasAnyPropertyFlags(CPF_ReturnParm) || Prop->HasAnyPropertyFlags(CPF_OutParm))
						{
							continue;
						}

						const FString ParamPinName = Prop->GetName();
						const FUEIRGraphPin* NodePin = UEIRInterchange_FindPinIR(*Node, ParamPinName, TEXT("Input"));
						if (!NodePin)
						{
							continue;
						}

						FUEIRGraphPinRef Src;
						FString ValueExpr;
						if (UEIRInterchange_TryResolveDataSource(DataSources, Node->Id, NodePin->Name, Src))
						{
							FString Err;
							FString DummyType;
							if (!UEIRInterchange_GenExprForPinRef(Src, Graph, NodeMap, EntryParamNameMap, LocalVarNameMap, DataSources, C, ForwardDecls, PureCallTempByOutputPin, CallIndex, ValueExpr, Err))
							{
								ValueExpr = UEIRInterchange_CppLiteralFromPinDefault(*NodePin);
							}
						}
						else
						{
							ValueExpr = UEIRInterchange_CppLiteralFromPinDefault(*NodePin);
						}

						C.Add(FString::Printf(TEXT("%s.%s = %s;"), *ParamsVar, *UEIRInterchange_SanitizeIdentifier(Prop->GetName()), *ValueExpr));
					}

					C.Add(FString::Printf(TEXT("UObject* UEIR_Target = %s->HasAnyFunctionFlags(FUNC_Static) ? %s->GetOuterUClass()->GetDefaultObject() : Context;"), *FuncVar, *FuncVar));
					C.Add(TEXT("if (UEIR_Target)"));
					C.Add(TEXT("{"));
					C.Push();
					C.Add(FString::Printf(TEXT("UEIR_Target->ProcessEvent(%s, &%s);"), *FuncVar, *ParamsVar));
					C.Pop();
					C.Add(TEXT("}"));

					C.Pop();
					C.Add(TEXT("}"));

					const FString Next = UEIRInterchange_GetExecNextNodeId(ExecNext, Node->Id, UEdGraphSchema_K2::PN_Then.ToString());
					return Next.IsEmpty() ? true : GenStmt(Next);
				}

				if (ClassName == TEXT("K2Node_IfThenElse"))
				{
					const FUEIRGraphPin* CondPin = UEIRInterchange_FindPinIR(*Node, TEXT("Condition"), TEXT("Input"));
					FString CondExpr = TEXT("false");
					if (CondPin)
					{
						FUEIRGraphPinRef Src;
						if (UEIRInterchange_TryResolveDataSource(DataSources, Node->Id, CondPin->Name, Src))
						{
							FString Err;
							FString DummyType;
							if (!UEIRInterchange_GenExprForPinRef(Src, Graph, NodeMap, EntryParamNameMap, LocalVarNameMap, DataSources, C, ForwardDecls, PureCallTempByOutputPin, CallIndex, CondExpr, Err))
							{
								CondExpr = UEIRInterchange_CppLiteralFromPinDefault(*CondPin);
							}
						}
						else
						{
							CondExpr = UEIRInterchange_CppLiteralFromPinDefault(*CondPin);
						}
					}

					const FString ThenStart = UEIRInterchange_GetExecNextNodeId(ExecNext, Node->Id, UEdGraphSchema_K2::PN_Then.ToString());
					const FString ElseStart = UEIRInterchange_GetExecNextNodeId(ExecNext, Node->Id, UEdGraphSchema_K2::PN_Else.ToString());

					C.Add(FString::Printf(TEXT("if (%s)"), *CondExpr));
					C.Add(TEXT("{"));
					C.Push();
					if (!ThenStart.IsEmpty())
					{
						GenStmt(ThenStart);
					}
					C.Pop();
					C.Add(TEXT("}"));
					C.Add(TEXT("else"));
					C.Add(TEXT("{"));
					C.Push();
					if (!ElseStart.IsEmpty())
					{
						GenStmt(ElseStart);
					}
					C.Pop();
					C.Add(TEXT("}"));
					return true;
				}

				const FString Next = UEIRInterchange_GetExecNextNodeId(ExecNext, Node->Id, UEdGraphSchema_K2::PN_Then.ToString());
				return Next.IsEmpty() ? true : GenStmt(Next);
			};

			GenStmt(StartNodeId);
		}

		C.Pop();
		C.Add(TEXT("}"));
	}

	C.Pop();
	C.AddRaw(TEXT("}"));

	FString ForwardDeclText;
	if (ForwardDecls.Num() > 0)
	{
		TArray<FString> Sorted = ForwardDecls.Array();
		Sorted.Sort();
		for (const FString& D : Sorted)
		{
			ForwardDeclText += FString::Printf(TEXT("class %s;\n"), *D);
		}
		ForwardDeclText += TEXT("\n");
	}

	const FString HeaderText = FString::Join(TArray<FString>({ TEXT("#pragma once\n#include \"CoreMinimal.h\"\n"), ForwardDeclText, TEXT("namespace UEIRGenerated\n{\n\t"), Signature.IsEmpty() ? FString::Printf(TEXT("void %s(UObject* Context);"), *PublicFuncName) : Signature + TEXT(";"), TEXT("\n}\n") }), TEXT(""));

	const FString OutDir = OutputDir.IsEmpty() ? UEIRInterchange_GetDefaultUEIRGeneratedOutputDir() : OutputDir;
	const FString SafeBlueprint = FPaths::MakeValidFileName(BlueprintName);
	const FString SafeFunc = FPaths::MakeValidFileName(FunctionGraphName);

	OutHeaderFile = FPaths::Combine(OutDir, FString::Printf(TEXT("%s_%s.generated.h"), *SafeBlueprint, *SafeFunc));
	OutCppFile = FPaths::Combine(OutDir, FString::Printf(TEXT("%s_%s.generated.cpp"), *SafeBlueprint, *SafeFunc));

	if (!UEIRInterchange_WriteTextFileEnsureDir(OutHeaderFile, HeaderText))
	{
		return false;
	}
	if (!UEIRInterchange_WriteTextFileEnsureDir(OutCppFile, C.ToString()))
	{
		return false;
	}

	OutReportMarkdown += TEXT("# UEIR C++ Generation Report\n\n");
	OutReportMarkdown += FString::Printf(TEXT("- Blueprint: %s\n"), *BlueprintName);
	OutReportMarkdown += FString::Printf(TEXT("- FunctionGraph: %s\n"), *FunctionGraphName);
	OutReportMarkdown += FString::Printf(TEXT("- Header: %s\n"), *FPaths::ConvertRelativePathToFull(OutHeaderFile));
	OutReportMarkdown += FString::Printf(TEXT("- Cpp: %s\n"), *FPaths::ConvertRelativePathToFull(OutCppFile));
	OutReportMarkdown += FString::Printf(TEXT("- Degraded: %s\n"), bDegraded ? TEXT("Yes") : TEXT("No"));
	OutReportMarkdown += TEXT("\n");

	if (bDegraded)
	{
		OutReportMarkdown += TEXT("## Reasons\n\n");
		for (const FString& R : Reasons)
		{
			OutReportMarkdown += FString::Printf(TEXT("- %s\n"), *R);
		}
		OutReportMarkdown += TEXT("\n");
	}

	return true;
}

bool UUEIRInterchangeBPLibrary::GenerateCppFromBlueprintFunctionGraph(const UBlueprint* Blueprint, const FString& FunctionGraphName, const FString& OutputDir, FString& OutHeaderFile, FString& OutCppFile, FString& OutReportMarkdown)
{
	OutHeaderFile.Reset();
	OutCppFile.Reset();
	OutReportMarkdown.Reset();

	if (!IsValid(Blueprint) || FunctionGraphName.IsEmpty())
	{
		return false;
	}

	FUEIRBlueprintFunctionGraph GraphIR;
	FUEIRBlueprintGraphConversionReport ExportReport;
	if (!ExportBlueprintFunctionGraphToUEIR(Blueprint, FunctionGraphName, GraphIR, ExportReport))
	{
		OutReportMarkdown = BlueprintGraphReportToMarkdown(ExportReport);
		return false;
	}

	const bool bOk = UEIRInterchange_GenerateCppFromGraphIR(Blueprint->GetName(), FunctionGraphName, GraphIR, OutputDir, OutHeaderFile, OutCppFile, OutReportMarkdown);
	if (!bOk)
	{
		return false;
	}

	const FString OutDir = OutputDir.IsEmpty() ? UEIRInterchange_GetDefaultUEIRGeneratedOutputDir() : OutputDir;
	const FString SafeBlueprint = FPaths::MakeValidFileName(Blueprint->GetName());
	const FString SafeFunc = FPaths::MakeValidFileName(FunctionGraphName);
	const FString ReportFile = FPaths::Combine(OutDir, FString::Printf(TEXT("%s_%s.codegen.md"), *SafeBlueprint, *SafeFunc));
	UEIRInterchange_WriteTextFileEnsureDir(ReportFile, OutReportMarkdown);
	return true;
}

bool UUEIRInterchangeBPLibrary::GenerateCppFromUEIRBlueprintFunctionGraph(const FString& BlueprintName, const FString& FunctionGraphName, const FUEIRBlueprintFunctionGraph& Graph, const FString& OutputDir, FString& OutHeaderFile, FString& OutCppFile, FString& OutReportMarkdown)
{
	OutHeaderFile.Reset();
	OutCppFile.Reset();
	OutReportMarkdown.Reset();

	if (BlueprintName.IsEmpty() || FunctionGraphName.IsEmpty() || Graph.Nodes.Num() == 0)
	{
		return false;
	}

	const bool bOk = UEIRInterchange_GenerateCppFromGraphIR(BlueprintName, FunctionGraphName, Graph, OutputDir, OutHeaderFile, OutCppFile, OutReportMarkdown);
	if (!bOk)
	{
		return false;
	}

	const FString OutDir = OutputDir.IsEmpty() ? UEIRInterchange_GetDefaultUEIRGeneratedOutputDir() : OutputDir;
	const FString SafeBlueprint = FPaths::MakeValidFileName(BlueprintName);
	const FString SafeFunc = FPaths::MakeValidFileName(FunctionGraphName);
	const FString ReportFile = FPaths::Combine(OutDir, FString::Printf(TEXT("%s_%s.codegen.md"), *SafeBlueprint, *SafeFunc));
	UEIRInterchange_WriteTextFileEnsureDir(ReportFile, OutReportMarkdown);
	return true;
}

bool UUEIRInterchangeBPLibrary::GenerateBlueprintWrapperFunctionGraphFromNativeFunction(UBlueprint* TargetBlueprint, const FString& WrapperGraphName, const FString& NativeFunctionPath, bool bReplaceIfExists, FUEIRBlueprintGraphConversionReport& OutReport)
{
	OutReport = UEIRInterchange_MakeEmptyReport();

	if (!IsValid(TargetBlueprint) || WrapperGraphName.IsEmpty() || NativeFunctionPath.IsEmpty())
	{
		OutReport.Errors.Add(TEXT("TargetBlueprint/WrapperGraphName/NativeFunctionPath invalid"));
		return false;
	}

	UFunction* NativeFunc = Cast<UFunction>(StaticLoadObject(UFunction::StaticClass(), nullptr, *NativeFunctionPath));
	if (!IsValid(NativeFunc))
	{
		OutReport.Errors.Add(FString::Printf(TEXT("Native function not found: %s"), *NativeFunctionPath));
		return false;
	}

	if (bReplaceIfExists)
	{
		if (!UEIRInterchange_RemoveExistingFunctionGraph(TargetBlueprint, WrapperGraphName))
		{
			OutReport.Errors.Add(TEXT("Failed to remove existing wrapper function graph"));
			return false;
		}
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(TargetBlueprint, FName(*WrapperGraphName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!IsValid(NewGraph))
	{
		OutReport.Errors.Add(TEXT("Failed to create wrapper function graph"));
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph(TargetBlueprint, NewGraph, false, NativeFunc);

	UK2Node_FunctionEntry* EntryNode = nullptr;
	UK2Node_FunctionResult* ReturnNode = nullptr;
	for (UEdGraphNode* N : NewGraph->Nodes)
	{
		if (!EntryNode)
		{
			EntryNode = Cast<UK2Node_FunctionEntry>(N);
		}
		if (!ReturnNode)
		{
			ReturnNode = Cast<UK2Node_FunctionResult>(N);
		}
	}

	if (!EntryNode || !ReturnNode)
	{
		OutReport.Errors.Add(TEXT("Entry/Return node missing in created wrapper graph"));
		return false;
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(NewGraph);
	CallNode->SetFromFunction(NativeFunc);
	CallNode->CreateNewGuid();
	CallNode->PostPlacedNewNode();
	CallNode->AllocateDefaultPins();
	CallNode->NodePosX = 350;
	CallNode->NodePosY = 0;
	NewGraph->AddNode(CallNode, true, false);

	const UEdGraphSchema* Schema = NewGraph->GetSchema();
	if (!Schema)
	{
		OutReport.Errors.Add(TEXT("Graph schema missing"));
		return false;
	}

	UEdGraphPin* EntryThen = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
	UEdGraphPin* CallExec = CallNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	UEdGraphPin* CallThen = CallNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
	UEdGraphPin* RetExec = ReturnNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	if (EntryThen && CallExec)
	{
		Schema->TryCreateConnection(EntryThen, CallExec);
	}
	if (CallThen && RetExec)
	{
		Schema->TryCreateConnection(CallThen, RetExec);
	}

	for (UEdGraphPin* P : EntryNode->Pins)
	{
		if (!P || P->Direction != EGPD_Output || P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}
		if (UEdGraphPin* CallPin = CallNode->FindPin(P->PinName, EGPD_Input))
		{
			Schema->TryCreateConnection(P, CallPin);
		}
	}

	for (UEdGraphPin* P : ReturnNode->Pins)
	{
		if (!P || P->Direction != EGPD_Input || P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}
		if (UEdGraphPin* CallOut = CallNode->FindPin(P->PinName, EGPD_Output))
		{
			Schema->TryCreateConnection(CallOut, P);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(TargetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(TargetBlueprint);
	OutReport.bSuccess = true;
	return true;
}

bool UUEIRInterchangeBPLibrary::SaveUEIRExecutionResultToJsonFile(const FUEIRExecutionResultJson& Result, const FString& OutputFile, FString& OutJson)
{
	OutJson.Reset();
	if (OutputFile.IsEmpty())
	{
		return false;
	}

	if (!FJsonObjectConverter::UStructToJsonObjectString(Result, OutJson, 0, 0, 0, nullptr, true))
	{
		return false;
	}

	return UEIRInterchange_WriteTextFileEnsureDir(OutputFile, OutJson);
}
