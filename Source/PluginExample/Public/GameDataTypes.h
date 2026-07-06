#pragma once

#include "CoreMinimal.h"
#include "GameDataTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EChunkDirection : uint8
{
    North   UMETA(DisplayName = "North"),
    East    UMETA(DisplayName = "East"),
    South   UMETA(DisplayName = "South"),
    West    UMETA(DisplayName = "West")
};

/** Exit point for a chunk */
USTRUCT(BlueprintType)
struct FExitPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ExitId;

    /** Relative location within chunk (deprecated - computed from direction) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector RelativeLocation = FVector::ZeroVector;

    /** Direction the player faces when exiting (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExitDirection = 0.0f;

    /** Target chunk ID to transition to (empty if not yet generated) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetChunkId;

    /** Whether this exit leads to a locked area */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsLocked = false;

    /** Condition to unlock this exit */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString UnlockCondition;
};

/** NPC data stored in chunk */
USTRUCT(BlueprintType)
struct FNPCData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NPCId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Personality;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DialogueStyle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Faction;

    /** Dialogue lines or conversation tree reference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InitialDialogue;

    /** Position relative to chunk */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector RelativeLocation = FVector::ZeroVector;

    /** Rotation Yaw */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RotationYaw = 0.0f;

    /** Optional portrait texture */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> Portrait;
};

/** Game chunk data */
USTRUCT(BlueprintType)
struct FChunkData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ChunkId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    /** Chunk type: Village, Town, Forest, Dungeon, etc. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ChunkType;

    /** NPCs in this chunk */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FNPCData> NPCs;

    /** Exit points */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FExitPoint> ExitPoints;

    /** Chunk size in world units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChunkSize = 1000.0f;

    /** Whether this chunk has been discovered */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsDiscovered = false;

    /** Tags for this chunk */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;
};

/** World configuration from initial prompt */
USTRUCT(BlueprintType)
struct FWorldConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString WorldId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString WorldName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Era;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Geography;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Factions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Rules;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StoryTheme;

    /** Raw prompt used to generate this world */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OriginalPrompt;

    /** Structured player appearance description for character image generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlayerAppearance;
};

/** Dialogue action types that can be triggered by NPC dialogue */
UENUM(BlueprintType)
enum class EDialogueActionType : uint8
{
    None            UMETA(DisplayName = "None"),
    EndDialogue     UMETA(DisplayName = "End Dialogue"),
    UnlockExit      UMETA(DisplayName = "Unlock Exit"),
    GiveItem        UMETA(DisplayName = "Give Item"),
    StartQuest      UMETA(DisplayName = "Start Quest"),
    ChangeNPCFaction UMETA(DisplayName = "Change NPC Faction"),
    Custom          UMETA(DisplayName = "Custom")
};

/** Dialogue action triggered during conversation */
USTRUCT(BlueprintType)
struct FDialogueAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDialogueActionType ActionType = EDialogueActionType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ExitId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetChunkId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString QuestId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NPCId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NewFaction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CustomData;
};
