#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class ANPCCharacter;
class UInputAction;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

UCLASS()
class PLUGINEXAMPLE_API APlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    APlayerCharacter();

    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    /** Get the NPC currently in range */
    UFUNCTION(BlueprintPure)
    ANPCCharacter* GetNearbyNPC() const { return NearbyNPC; }

    /** Check if player can interact with nearby NPC */
    UFUNCTION(BlueprintPure)
    bool CanInteract() const { return NearbyNPC != nullptr; }

    /** Get stable player id used for image generation caching */
    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    FString GetPlayerImageId() const { return TEXT("Player"); }

    /** Apply generated pixel-art texture to player billboard */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void SetSpriteFromTexture(UTexture2D* InTexture);

    /** Set walking animation frames (2 frames that will cycle when moving) */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void SetWalkFrames(UTexture2D* Frame1, UTexture2D* Frame2);

    /** Get current pixel texture */
    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    UTexture2D* GetCurrentPixelTexture() const { return CurrentPixelTexture; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    void MoveForward(float Value);
    void MoveRight(float Value);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void OnInteractPressed();

    void CheckNearbyNPCs();

    /** Enhanced Input - Jump action */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> InputActionJump;

    /** Enhanced Input - Move forward action (Vector2D) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> InputActionMoveForward;

    /** Enhanced Input - Move right action (Vector2D) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> InputActionMoveRight;

    /** Enhanced Input - Interact action */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> InputActionInteract;

    /** Enhanced Input - Input Mapping Context to activate */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<class UInputMappingContext> InputMappingContext;

private:
    /** Enhanced Input callbacks */
    void OnJumpStarted(const FInputActionValue& Value);
    void OnJumpEnded(const FInputActionValue& Value);
    void OnMoveForward(const FInputActionValue& Value);
    void OnMoveRight(const FInputActionValue& Value);
    void OnInteract(const FInputActionValue& Value);

public:
    /** Camera boom */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<USpringArmComponent> CameraBoom;

    /** Follow camera */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UCameraComponent> FollowCamera;

    /** Interaction radius */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InteractionRadius = 200.0f;

    /** Static mesh component for in-world player display (plane with texture) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RPG|CharacterImage")
    TObjectPtr<UStaticMeshComponent> CharacterMesh;

    /** Dynamic material instance for texture switching */
    UPROPERTY(BlueprintReadOnly, Category = "RPG|CharacterImage")
    TObjectPtr<UMaterialInstanceDynamic> CharacterMaterial;

    /** Current displayed pixel texture */
    UPROPERTY(BlueprintReadOnly, Category = "RPG|CharacterImage")
    TObjectPtr<UTexture2D> CurrentPixelTexture;

    /** Walking animation frame textures */
    UPROPERTY(BlueprintReadOnly, Category = "RPG|CharacterImage")
    TArray<TObjectPtr<UTexture2D>> WalkFrameTextures;

    /** Current walking frame index */
    UPROPERTY()
    int32 CurrentWalkFrame = 0;

    /** Timer handle for walk animation */
    UPROPERTY()
    FTimerHandle WalkAnimTimer;

private:
    UPROPERTY()
    ANPCCharacter* NearbyNPC;
};
