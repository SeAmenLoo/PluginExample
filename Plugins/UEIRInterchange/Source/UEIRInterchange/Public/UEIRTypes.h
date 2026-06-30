#pragma once

#include "CoreMinimal.h"
#include "UEIRTypes.generated.h"

UENUM(BlueprintType)
enum class EUEIRItemKind : uint8
{
	Asset UMETA(DisplayName = "Asset"),
	Blueprint UMETA(DisplayName = "Blueprint"),
	Class UMETA(DisplayName = "Class")
};

USTRUCT(BlueprintType)
struct FUEIRVariable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Container;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SubTypeObjectPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEditable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bExposeOnSpawn = false;
};

USTRUCT(BlueprintType)
struct FUEIRBlueprintSummary
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ParentClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRVariable> Variables;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> FunctionGraphNames;
};

USTRUCT(BlueprintType)
struct FUEIRItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUEIRItemKind Kind = EUEIRItemKind::Asset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ObjectPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUEIRBlueprintSummary Blueprint;
};

USTRUCT(BlueprintType)
struct FUEIRMeta
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ExportedAtUtcIso8601;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EngineVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ProjectName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PluginName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PluginVersion;
};

USTRUCT(BlueprintType)
struct FUEIRDocument
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SchemaId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Version;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUEIRMeta Meta;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRItem> Items;
};

USTRUCT(BlueprintType)
struct FUEIRGraphPinType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SubCategory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SubCategoryObjectPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Container;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsReference = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsConst = false;
};

USTRUCT(BlueprintType)
struct FUEIRGraphPin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUEIRGraphPinType Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DefaultValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DefaultObjectPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DefaultText;
};

USTRUCT(BlueprintType)
struct FUEIRGraphNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PosX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PosY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRGraphPin> Pins;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CallFunctionPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString VariableName;
};

USTRUCT(BlueprintType)
struct FUEIRGraphPinRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PinName;
};

USTRUCT(BlueprintType)
struct FUEIRGraphEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUEIRGraphPinRef From;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUEIRGraphPinRef To;
};

USTRUCT(BlueprintType)
struct FUEIRBlueprintFunctionGraph
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString GraphName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRGraphNode> Nodes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUEIRGraphEdge> Edges;
};

USTRUCT(BlueprintType)
struct FUEIRBlueprintGraphConversionReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> UnsupportedNodeClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Warnings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Errors;
};
