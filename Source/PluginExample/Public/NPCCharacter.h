#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameDataTypes.h"
#include "Components/TextRenderComponent.h"
#include "NPCCharacter.generated.h"

class USphereComponent;
class UWidgetComponent;
class UDialogueWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNPCInteractSignature, ANPCCharacter*, NPC, bool, bIsPlayerInRange);

UCLASS()
class PLUGINEXAMPLE_API ANPCCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ANPCCharacter();

    void SetNPCData(const FNPCData& InData);
    FNPCData GetNPCData() const { return NPCData; }
    FString GetNPCId() const { return NPCData.NPCId; }

    /** Called when player enters interaction range */
    UPROPERTY(BlueprintAssignable)
    FNPCInteractSignature OnPlayerEnterRange;

    /** Called when player exits interaction range */
    UPROPERTY(BlueprintAssignable)
    FNPCInteractSignature OnPlayerExitRange;

    /** Start dialogue with this NPC */
    UFUNCTION(BlueprintCallable)
    void StartDialogue();

    /** End current dialogue */
    UFUNCTION(BlueprintCallable)
    void EndDialogue();

    UFUNCTION(BlueprintPure)
    bool IsInDialogue() const { return bIsInDialogue; }

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnInteractionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UFUNCTION()
    void OnInteractPressed();

private:
    void UpdateInteractionPrompt();

public:
    /** Interaction range sphere */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<USphereComponent> InteractionSphere;

    /** NPC name display above head */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UTextRenderComponent> NPCNameLabel;

    /** Interaction prompt widget */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UWidgetComponent> InteractionPromptWidget;

    /** Interaction key to display */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InteractionKey = "E";

    /** Dialogue widget class to use */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<UDialogueWidget> DialogueWidgetClass;

private:
    UPROPERTY()
    FNPCData NPCData;

    UPROPERTY()
    bool bIsInDialogue = false;

    UPROPERTY()
    bool bPlayerInRange = false;
};
