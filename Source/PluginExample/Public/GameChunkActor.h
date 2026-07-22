#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameDataTypes.h"
#include "GameChunkActor.generated.h"

class UBoxComponent;
class USceneComponent;
class UTextRenderComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
class ANPCCharacter;
class APortalActor;

UCLASS()
class PLUGINEXAMPLE_API AGameChunkActor : public AActor
{
    GENERATED_BODY()

public:
    AGameChunkActor();

    void SetChunkData(const FChunkData& InData);

    FChunkData GetChunkData() const { return ChunkData; }

    FString GetChunkId() const { return ChunkData.ChunkId; }

    /** Get the player start transform for entering this chunk from a specific exit */
    FTransform GetPlayerStartTransform() const;

    /** Get player start transform when entering from a specific exit direction */
    FTransform GetPlayerStartTransformFromExit(float EntryExitDirection) const;

    /** Compute portal location on chunk edge based on direction */
    FVector ComputePortalLocation(float ExitDirection) const;

    /** Compute NPC spawn location at index within the chunk center area */
    FVector ComputeNPCLocation(int32 NPCIndex, int32 TotalNPCs) const;

    /** Portal actor class to spawn for exits */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
    TSubclassOf<APortalActor> PortalClass;

    /** Offset from chunk edge for portal placement (inside the chunk) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Layout")
    float PortalEdgeOffset = 50.0f;

    /** Radius of the center area where NPCs spawn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Layout")
    float NPCCenterAreaRadius = 300.0f;

    /** Spacing between NPCs when multiple are present */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Layout")
    float NPCSpacing = 200.0f;

    /** Z-axis offset for player spawn and portal positions (to avoid falling through the floor) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Layout")
    float SpawnZOffset = 150.0f;

    /** How far inside the chunk the player should spawn after arriving from a portal */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Layout")
    float PlayerSpawnOffsetFromPortal = 160.0f;

    /** Rebuild exit portals based on current chunk data */
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void RebuildExitPortals();

    /** Create portal for a specific exit (only if target chunk exists) */
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void CreatePortalForExit(const FExitPoint& Exit);

    /** Create portal by exit ID (only if target chunk exists) */
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void CreatePortalForExitById(const FString& ExitId);

    /** Update exit point's target chunk ID (called after new chunk is generated) */
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void UpdateExitTargetChunkId(const FString& ExitId, const FString& TargetChunkId);

    /** Auto-set reverse exit's target chunk ID based on entry direction */
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    void SetReverseExitTarget(const FString& SourceChunkId, float EntryDirection);

    /** Apply a generated ground texture to the chunk mesh. Pass nullptr to hide the ground. */
    UFUNCTION(BlueprintCallable, Category = "Chunk|GroundTexture")
    void SetGroundTexture(UTexture2D* InTexture);

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandlePortalEntered(APortalActor* Portal, AActor* OtherActor);

public:
    /** Visual size of the chunk */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChunkSize = 1000.0f;

    /** Base mesh for the chunk (used as ground plane) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UStaticMeshComponent> ChunkMesh;

    /** Portal actors spawned for exits */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TObjectPtr<APortalActor>> ExitPortals;

    /** NPC actors spawned in this chunk */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TObjectPtr<ANPCCharacter>> SpawnedNPCs;

    /** Chunk name display */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UTextRenderComponent> ChunkNameLabel;

    /** Plane mesh asset used for the ground (defaults to engine BasicShapes/Plane) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|GroundTexture")
    FSoftObjectPath GroundMeshAsset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane"));

    /** Base material for the ground (unlit, supports a texture parameter named by GroundTextureParameterName) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|GroundTexture")
    FSoftObjectPath GroundBaseMaterial = FSoftObjectPath(TEXT("/Paper2D/MaskedUnlitSpriteMaterial"));

    /** Texture parameter name inside GroundBaseMaterial that will receive the generated ground texture */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|GroundTexture")
    FName GroundTextureParameterName = TEXT("SpriteTexture");

    /** Dynamic material instance for ground texture switching */
    UPROPERTY(BlueprintReadOnly, Category = "Chunk|GroundTexture")
    TObjectPtr<UMaterialInstanceDynamic> GroundMaterial;

    /** Currently applied ground texture */
    UPROPERTY(BlueprintReadOnly, Category = "Chunk|GroundTexture")
    TObjectPtr<UTexture2D> CurrentGroundTexture;

private:
    UPROPERTY()
    FChunkData ChunkData;

    float LastExitTriggerTime = -1.0f;

    static constexpr float ExitTriggerCooldown = 3.0f;
};
