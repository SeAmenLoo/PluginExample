#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class ANPCCharacter;
class UInputAction;

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

private:
    UPROPERTY()
    ANPCCharacter* NearbyNPC;
};
