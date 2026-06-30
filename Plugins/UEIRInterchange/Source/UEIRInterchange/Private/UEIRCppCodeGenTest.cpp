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
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUEIRCppCodeGenTest, "UEIRInterchange.CodeGen.CppFromBlueprintFunctionGraph", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

static void UEIRInterchange_DeletePackageFiles2(const FString& LongPackageName)
{
	const FString AssetFilename = FPackageName::LongPackageNameToFilename(LongPackageName, FPackageName::GetAssetPackageExtension());
	const FString ExpFilename = FPaths::ChangeExtension(AssetFilename, TEXT("uexp"));
	const FString BulkFilename = FPaths::ChangeExtension(AssetFilename, TEXT("ubulk"));

	IFileManager::Get().Delete(*AssetFilename, false, true, true);
	IFileManager::Get().Delete(*ExpFilename, false, true, true);
	IFileManager::Get().Delete(*BulkFilename, false, true, true);
}

static void UEIRInterchange_UnloadPackage2(UPackage* Package)
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

static UEdGraphPin* UEIRInterchange_FindExecPin2(UEdGraphNode* Node, EEdGraphPinDirection Dir, const FName& PreferredName = NAME_None)
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

static UEdGraphPin* UEIRInterchange_FindFirstDataOutputPin2(UEdGraphNode* Node)
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

static void UEIRInterchange_AddNode2(UEdGraph* Graph, UEdGraphNode* Node)
{
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	Graph->AddNode(Node, true, false);
}

bool FUEIRCppCodeGenTest::RunTest(const FString& Parameters)
{
	const FString PackagePath = TEXT("/Game/UEIRInterchangeTests");
	const FString SrcName = TEXT("BP_CppGen_Src");
	const FString FuncName = TEXT("TestFunc");

	const FString SrcLongPackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *SrcName);
	UEIRInterchange_DeletePackageFiles2(SrcLongPackageName);

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
	UEIRInterchange_AddNode2(FuncGraph, VarGet);

	UK2Node_IfThenElse* IfNode = NewObject<UK2Node_IfThenElse>(FuncGraph);
	IfNode->NodePosX = 400;
	IfNode->NodePosY = 0;
	UEIRInterchange_AddNode2(FuncGraph, IfNode);

	UFunction* PrintStringFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(TEXT("PrintString")));
	TestNotNull(TEXT("PrintStringFunc"), PrintStringFunc);

	UK2Node_CallFunction* CallThen = NewObject<UK2Node_CallFunction>(FuncGraph);
	CallThen->SetFromFunction(PrintStringFunc);
	CallThen->NodePosX = 650;
	CallThen->NodePosY = -80;
	UEIRInterchange_AddNode2(FuncGraph, CallThen);

	UK2Node_CallFunction* CallElse = NewObject<UK2Node_CallFunction>(FuncGraph);
	CallElse->SetFromFunction(PrintStringFunc);
	CallElse->NodePosX = 650;
	CallElse->NodePosY = 80;
	UEIRInterchange_AddNode2(FuncGraph, CallElse);

	const UEdGraphSchema* Schema = FuncGraph->GetSchema();
	TestNotNull(TEXT("GraphSchema"), Schema);

	UEdGraphPin* EntryThen = UEIRInterchange_FindExecPin2(EntryNode, EGPD_Output, UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* IfExec = UEIRInterchange_FindExecPin2(IfNode, EGPD_Input, UEdGraphSchema_K2::PN_Execute);
	TestNotNull(TEXT("EntryThen"), EntryThen);
	TestNotNull(TEXT("IfExec"), IfExec);
	Schema->TryCreateConnection(EntryThen, IfExec);

	UEdGraphPin* CondPin = IfNode->FindPin(FName(TEXT("Condition")), EGPD_Input);
	UEdGraphPin* VarValue = UEIRInterchange_FindFirstDataOutputPin2(VarGet);
	TestNotNull(TEXT("CondPin"), CondPin);
	TestNotNull(TEXT("VarValue"), VarValue);
	Schema->TryCreateConnection(VarValue, CondPin);

	UEdGraphPin* IfThen = UEIRInterchange_FindExecPin2(IfNode, EGPD_Output, UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* IfElse = UEIRInterchange_FindExecPin2(IfNode, EGPD_Output, UEdGraphSchema_K2::PN_Else);
	UEdGraphPin* ThenExec = UEIRInterchange_FindExecPin2(CallThen, EGPD_Input, UEdGraphSchema_K2::PN_Execute);
	UEdGraphPin* ElseExec = UEIRInterchange_FindExecPin2(CallElse, EGPD_Input, UEdGraphSchema_K2::PN_Execute);
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

	UEdGraphPin* ThenOut = UEIRInterchange_FindExecPin2(CallThen, EGPD_Output, UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* ElseOut = UEIRInterchange_FindExecPin2(CallElse, EGPD_Output, UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* RetExec = UEIRInterchange_FindExecPin2(ReturnNode, EGPD_Input, UEdGraphSchema_K2::PN_Execute);
	TestNotNull(TEXT("ThenOut"), ThenOut);
	TestNotNull(TEXT("ElseOut"), ElseOut);
	TestNotNull(TEXT("RetExec"), RetExec);
	Schema->TryCreateConnection(ThenOut, RetExec);
	Schema->TryCreateConnection(ElseOut, RetExec);

	FKismetEditorUtilities::CompileBlueprint(SrcBP);

	FString HeaderFile;
	FString CppFile;
	FString ReportMarkdown;
	const bool bGenOk = UUEIRInterchangeBPLibrary::GenerateCppFromBlueprintFunctionGraph(SrcBP, FuncName, FString(), HeaderFile, CppFile, ReportMarkdown);
	TestTrue(TEXT("GenerateCppFromBlueprintFunctionGraph"), bGenOk);
	TestTrue(TEXT("HeaderExists"), IFileManager::Get().FileExists(*HeaderFile));
	TestTrue(TEXT("CppExists"), IFileManager::Get().FileExists(*CppFile));

	FString HeaderText;
	FString CppText;
	TestTrue(TEXT("LoadHeader"), FFileHelper::LoadFileToString(HeaderText, *HeaderFile));
	TestTrue(TEXT("LoadCpp"), FFileHelper::LoadFileToString(CppText, *CppFile));
	TestTrue(TEXT("HeaderHasNamespace"), HeaderText.Contains(TEXT("namespace UEIRGenerated")));
	TestTrue(TEXT("CppHasIf"), CppText.Contains(TEXT("if (")));
	TestTrue(TEXT("CppHasProcessEvent"), CppText.Contains(TEXT("ProcessEvent")));

	IFileManager::Get().Delete(*HeaderFile, false, true, true);
	IFileManager::Get().Delete(*CppFile, false, true, true);

	UEIRInterchange_UnloadPackage2(SrcPackage);
	UEIRInterchange_DeletePackageFiles2(SrcLongPackageName);
	return true;
}

#endif

