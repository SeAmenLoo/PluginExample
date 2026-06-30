#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortalActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPortalOverlapSignature, APortalActor*, Portal, AActor*, OtherActor);

/**
 * Portal actor for chunk transitions.
 * Can be subclassed in Blueprint to customize appearance and behavior.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class PLUGINEXAMPLE_API APortalActor : public AActor
{
    GENERATED_BODY()

public:
    APortalActor();

    /** Portal mesh component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    TObjectPtr<UStaticMeshComponent> PortalMesh;

    /** Collision trigger component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    TObjectPtr<UBoxComponent> TriggerBox;

    /** Portal name display */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    TObjectPtr<UTextRenderComponent> PortalLabel;

    /** Portal ID for routing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FString PortalId;

    /** Direction this portal leads to (in degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    float ExitDirection = 0.0f;

    /** Target chunk ID (optional, for pre-defined destinations) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FString TargetChunkId;

    /** Whether this portal is locked */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    bool bIsLocked = false;

    /** Unlock condition (e.g., item required, quest completed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal", meta=(EditCondition="bIsLocked"))
    FString UnlockCondition;

    /** Called when player enters the portal trigger */
    UPROPERTY(BlueprintAssignable, Category = "Portal")
    FOnPortalOverlapSignature OnPlayerEntered;

    /** Set the portal label text (e.g., target chunk name) */
    UFUNCTION(BlueprintCallable, Category = "Portal")
    void SetPortalLabel(const FString& LabelText);

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    /** Blueprint event when player enters portal */
    UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
    void OnPlayerEnterPortal(AActor* Player);

    /** Check if portal can be used */
    UFUNCTION(BlueprintCallable, Category = "Portal")
    bool CanUsePortal() const;

    /** Check if an actor is a player-controlled pawn */
    UFUNCTION(BlueprintCallable, Category = "Portal")
    bool IsPlayerPawn(AActor* OtherActor) const;
};
