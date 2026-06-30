#include "UEIRInterchangeBPLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUEIRBlueprintGraphIRRoundtripTest, "UEIRInterchange.BlueprintGraph.FunctionGraphRoundtrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

static void UEIRInterchange_DeletePackageFiles(const FString& LongPackageName)
{
	const FString AssetFilename = FPackageName::LongPackageNameToFilename(LongPackageName, FPackageName::GetAssetPackageExtension());
	const FString ExpFilename = FPaths::ChangeExtension(AssetFilename, TEXT("uexp"));
	const FString BulkFilename = FPaths::ChangeExtension(AssetFilename, TEXT("ubulk"));

	IFileManager::Get().Delete(*AssetFilename, false, true, true);
	IFileManager::Get().Delete(*ExpFilename, false, true, true);
	IFileManager::Get().Delete(*BulkFilename, false, true, true);
}

static void UEIRInterchange_UnloadPackage(UPackage* Package)
{
	if (!IsValid(Package))
	{
		return;
	}

	TArray<UPackage*> PackagesToUnload;
	PackagesToUnload.Add(Package);
	UPackageTools::UnloadPackages(PackagesToUnload);
	CollectGarbage(RF_NoFlags);
}

static UEdGraphPin* UEIRInterchange_FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Dir, const FName& PreferredName = NAME_None)
{
	if (!IsValid(Node))
	{
		return nullptr;
	}

	if (PreferredName != NAME_None)
	{
		if (UEdGraphPin* P = Node->FindPin(PreferredName, Dir))
		{
			return P;
		}
	}

	for (UEdGraphPin* P : Node->Pins)
	{
		if (!P)
		{
			continue;
		}
		if (P->Direction == Dir && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			return P;
		}
	}
	return nullptr;
}

static UEdGraphPin* UEIRInterchange_FindFirstDataOutputPin(UEdGraphNode* Node)
{
	if (!IsValid(Node))
	{
		return nullptr;
	}

	for (UEdGraphPin* P : Node->Pins)
	{
		if (!P)
		{
			continue;
		}
		if (P->Direction == EGPD_Output && P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			return P;
		}
	}
	return nullptr;
}

static void UEIRInterchange_AddNode(UEdGraph* Graph, UEdGraphNode* Node)
{
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	Graph->AddNode(Node, true, false);
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

bool FUEIRBlueprintGraphIRRoundtripTest::RunTest(const FString& Parameters)
{
	const FString PackagePath = TEXT("/Game/UEIRInterchangeTests");
	const FString SrcName = TEXT("BP_GraphRoundtrip_Src");
	const FString DstName = TEXT("BP_GraphRoundtrip_Dst");
	const FString FuncName = TEXT("TestFunc");

	const FString SrcLongPackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *SrcName);
	const FString DstLongPackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *DstName);

	UEIRInterchange_DeletePackageFiles(SrcLongPackageName);
	UEIRInterchange_DeletePackageFiles(DstLongPackageName);

	UPackage* SrcPackage = CreatePackage(*SrcLongPackageName);
	TestNotNull(TEXT("CreatePackage(Src)"), SrcPackage);

	UBlueprint* SrcBP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), SrcPackage, FName(*SrcName), EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
	TestNotNull(TEXT("CreateBlueprint(Src)"), SrcBP);
	FAssetRegistryModule::AssetCreated(SrcBP);

	{
		FEdGraphPinType BoolType;
		BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		const bool bVarAdded = FBlueprintEditorUtils::AddMemberVariable(SrcBP, FName(TEXT("bFlag")), BoolType);
		TestTrue(TEXT("AddMemberVariable(bFlag)"), bVarAdded);
	}

	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(SrcBP, FName(*FuncName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	TestNotNull(TEXT("CreateNewGraph(Func)"), FuncGraph);
	FBlueprintEditorUtils::AddFunctionGraph(SrcBP, FuncGraph, false, static_cast<UFunction*>(nullptr));

	UK2Node_FunctionEntry* EntryNode = nullptr;
	UK2Node_FunctionResult* ReturnNode = nullptr;
	for (UEdGraphNode* N : FuncGraph->Nodes)
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
	TestNotNull(TEXT("EntryNode"), EntryNode);
	TestNotNull(TEXT("ReturnNode"), ReturnNode);

	UK2Node_VariableGet* VarGet = NewObject<UK2Node_VariableGet>(FuncGraph);
	VarGet->VariableReference.SetSelfMember(FName(TEXT("bFlag")));
	VarGet->NodePosX = 200;
	VarGet->NodePosY = 0;
	UEIRInterchange_AddNode(FuncGraph, VarGet);

	UK2Node_IfThenElse* IfNode = NewObject<UK2Node_IfThenElse>(FuncGraph);
	IfNode->NodePosX = 400;
	IfNode->NodePosY = 0;
	UEIRInterchange_AddNode(FuncGraph, IfNode);

	UFunction* PrintStringFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(TEXT("PrintString")));
	TestNotNull(TEXT("PrintStringFunc"), PrintStringFunc);

	UK2Node_CallFunction* CallThen = NewObject<UK2Node_CallFunction>(FuncGraph);
	CallThen->SetFromFunction(PrintStringFunc);
	CallThen->NodePosX = 650;
	CallThen->NodePosY = -80;
	UEIRInterchange_AddNode(FuncGraph, CallThen);

	UK2Node_CallFunction* CallElse = NewObject<UK2Node_CallFunction>(FuncGraph);
	CallElse->SetFromFunction(PrintStringFunc);
	CallElse->NodePosX = 650;
	CallElse->NodePosY = 80;
	UEIRInterchange_AddNode(FuncGraph, CallElse);

	const UEdGraphSchema* Schema = FuncGraph->GetSchema();
	TestNotNull(TEXT("GraphSchema"), Schema);

	UEdGraphPin* EntryThen = UEIRInterchange_FindExecPin(EntryNode, EGPD_Output, UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* IfExec = UEIRInterchange_FindExecPin(IfNode, EGPD_Input, UEdGraphSchema_K2::PN_Execute);
	TestNotNull(TEXT("EntryThen"), EntryThen);
	TestNotNull(TEXT("IfExec"), IfExec);
	Schema->TryCreateConnection(EntryThen, IfExec);

	UEdGraphPin* CondPin = IfNode->FindPin(FName(TEXT("Condition")), EGPD_Input);
	UEdGraphPin* VarValue = UEIRInterchange_FindFirstDataOutputPin(VarGet);
	TestNotNull(TEXT("CondPin"), CondPin);
	TestNotNull(TEXT("VarValue"), VarValue);
	Schema->TryCreateConnection(VarValue, CondPin);

	UEdGraphPin* IfThen = UEIRInterchange_FindExecPin(IfNode, EGPD_Output, UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* IfElse = UEIRInterchange_FindExecPin(IfNode, EGPD_Output, UEdGraphSchema_K2::PN_Else);
	UEdGraphPin* ThenExec = UEIRInterchange_FindExecPin(CallThen, EGPD_Input, UEdGraphSchema_K2::PN_Execute);
	UEdGraphPin* ElseExec = UEIRInterchange_FindExecPin(CallElse, EGPD_Input, UEdGraphSchema_K2::PN_Execute);
	TestNotNull(TEXT("IfThen"), IfThen);
	TestNotNull(TEXT("IfElse"), IfElse);
	TestNotNull(TEXT("ThenExec"), ThenExec);
	TestNotNull(TEXT("ElseExec"), ElseExec);
	Schema->TryCreateConnection(IfThen, ThenExec);
	Schema->TryCreateConnection(IfElse, ElseExec);

	UEdGraphPin* ThenString = CallThen->FindPin(FName(TEXT("InString")), EGPD_Input);
	UEdGraphPin* ElseString = CallElse->FindPin(FName(TEXT("InString")), EGPD_Input);
	TestNotNull(TEXT("ThenString"), ThenString);
	TestNotNull(TEXT("ElseString"), ElseString);
	if (ThenString)
	{
		ThenString->DefaultValue = TEXT("Then");
	}
	if (ElseString)
	{
		ElseString->DefaultValue = TEXT("Else");
	}

	UEdGraphPin* ThenOut = UEIRInterchange_FindExecPin(CallThen, EGPD_Output, UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* ElseOut = UEIRInterchange_FindExecPin(CallElse, EGPD_Output, UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* RetExec = UEIRInterchange_FindExecPin(ReturnNode, EGPD_Input, UEdGraphSchema_K2::PN_Execute);
	TestNotNull(TEXT("ThenOut"), ThenOut);
	TestNotNull(TEXT("ElseOut"), ElseOut);
	TestNotNull(TEXT("RetExec"), RetExec);
	Schema->TryCreateConnection(ThenOut, RetExec);
	Schema->TryCreateConnection(ElseOut, RetExec);

	FKismetEditorUtilities::CompileBlueprint(SrcBP);

	FUEIRBlueprintFunctionGraph GraphIR;
	FUEIRBlueprintGraphConversionReport ExportReport;
	const bool bExported = UUEIRInterchangeBPLibrary::ExportBlueprintFunctionGraphToUEIR(SrcBP, FuncName, GraphIR, ExportReport);
	TestTrue(TEXT("ExportBlueprintFunctionGraphToUEIR"), bExported);
	TestEqual(TEXT("ExportReport.Errors"), ExportReport.Errors.Num(), 0);
	TestEqual(TEXT("ExportReport.UnsupportedNodeClasses"), ExportReport.UnsupportedNodeClasses.Num(), 0);
	TestTrue(TEXT("ExportedGraphNameNotEmpty"), !GraphIR.GraphName.IsEmpty());
	TestTrue(TEXT("ExportedNodesNotEmpty"), GraphIR.Nodes.Num() > 0);

	UEIRInterchange_UnloadPackage(SrcPackage);

	UBlueprint* DstBP = nullptr;
	FString DstObjectPath;
	FUEIRBlueprintGraphConversionReport ImportReport;
	const bool bCreatedAndImported = UUEIRInterchangeBPLibrary::CreateBlueprintAndImportUEIRBlueprintFunctionGraph(PackagePath, DstName, AActor::StaticClass()->GetPathName(), GraphIR, DstBP, DstObjectPath, ImportReport);
	TestTrue(TEXT("CreateBlueprintAndImportUEIRBlueprintFunctionGraph"), bCreatedAndImported);
	TestNotNull(TEXT("DstBP"), DstBP);
	TestEqual(TEXT("ImportReport.Errors"), ImportReport.Errors.Num(), 0);
	TestEqual(TEXT("ImportReport.UnsupportedNodeClasses"), ImportReport.UnsupportedNodeClasses.Num(), 0);
	TestTrue(TEXT("DstObjectPathNotEmpty"), !DstObjectPath.IsEmpty());

	UEdGraph* ImportedGraph = nullptr;
	TestTrue(TEXT("FindImportedGraph"), DstBP && UEIRInterchange_FindBlueprintFunctionGraph(DstBP, FuncName, ImportedGraph));
	TestNotNull(TEXT("ImportedGraph"), ImportedGraph);

	if (ImportedGraph)
	{
		int32 CallFuncCount = 0;
		int32 IfCount = 0;
		int32 VarGetCount = 0;
		int32 EntryCount = 0;
		int32 ReturnCount = 0;

		for (UEdGraphNode* N : ImportedGraph->Nodes)
		{
			CallFuncCount += Cast<UK2Node_CallFunction>(N) ? 1 : 0;
			IfCount += Cast<UK2Node_IfThenElse>(N) ? 1 : 0;
			VarGetCount += Cast<UK2Node_VariableGet>(N) ? 1 : 0;
			EntryCount += Cast<UK2Node_FunctionEntry>(N) ? 1 : 0;
			ReturnCount += Cast<UK2Node_FunctionResult>(N) ? 1 : 0;
		}

		TestTrue(TEXT("ImportedGraphHasCallFunction"), CallFuncCount >= 2);
		TestTrue(TEXT("ImportedGraphHasIf"), IfCount >= 1);
		TestTrue(TEXT("ImportedGraphHasVarGet"), VarGetCount >= 1);
		TestTrue(TEXT("ImportedGraphHasEntry"), EntryCount >= 1);
		TestTrue(TEXT("ImportedGraphHasReturn"), ReturnCount >= 1);
	}

	UEIRInterchange_UnloadPackage(DstBP ? DstBP->GetOutermost() : nullptr);
	UEIRInterchange_DeletePackageFiles(SrcLongPackageName);
	UEIRInterchange_DeletePackageFiles(DstLongPackageName);

	return true;
}

#endif
