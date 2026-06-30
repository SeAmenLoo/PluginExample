#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BasePlayerController.generated.h"

class UWorldSetupWidget;
class ULoadingWidget;
class UChunkManagerComponent;
class UPluginGameInstance;

UCLASS()
class PLUGINEXAMPLE_API ABasePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ABasePlayerController();

    /** Show the world setup widget */
    UFUNCTION(BlueprintCallable)
    void ShowWorldSetup();

    /** Hide the world setup widget */
    UFUNCTION(BlueprintCallable)
    void HideWorldSetup();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnWorldSetupComplete(const FString& WorldPrompt);

    UFUNCTION()
    void OnWorldGenerationComplete();

    UFUNCTION()
    void OnWorldGenerationFailed(const FString& Error);

    void ShowLoadingScreen();
    void HideLoadingScreen();

public:
    /** World setup widget class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UWorldSetupWidget> WorldSetupWidgetClass;

    /** Loading screen widget class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<ULoadingWidget> LoadingWidgetClass;

private:
    UPROPERTY()
    TObjectPtr<UWorldSetupWidget> WorldSetupWidget;

    UPROPERTY()
    TObjectPtr<ULoadingWidget> LoadingWidget;

    UPROPERTY()
    TObjectPtr<UChunkManagerComponent> ChunkManager;

    UPROPERTY()
    TObjectPtr<UPluginGameInstance> PluginGameInstance;
};
