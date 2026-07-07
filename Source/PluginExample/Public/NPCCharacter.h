#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameDataTypes.h"
#include "Components/TextRenderComponent.h"
#include "NPCCharacter.generated.h"

class USphereComponent;
class UWidgetComponent;
class UDialogueWidget;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

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

    /** Update NPC visual sprite from a single texture */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void SetSpriteFromTexture(UTexture2D* InTexture, bool bUseAsPixelArt = true);

    /** Get the current pixel texture */
    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    UTexture2D* GetCurrentPixelTexture() const { return CurrentPixelTexture; }

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
    void RebuildVisualComponent();

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

    /** Static mesh component that renders the pixel-art NPC visual used in-world */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RPG|CharacterImage")
    TObjectPtr<UStaticMeshComponent> CharacterMesh;

    /** Dynamic material instance for texture switching */
    UPROPERTY(BlueprintReadOnly, Category = "RPG|CharacterImage")
    TObjectPtr<UMaterialInstanceDynamic> CharacterMaterial;

    /** Cached pixel texture (assigned by CharacterImageSubsystem) */
    UPROPERTY(BlueprintReadOnly, Category = "RPG|CharacterImage")
    TObjectPtr<UTexture2D> CurrentPixelTexture;

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
