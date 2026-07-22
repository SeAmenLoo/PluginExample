#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BasePlayerController.generated.h"

class UWorldSetupWidget;
class ULoadingWidget;
class UAttributePanelWidget;
class UNotificationWidget;
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

    /** Toggle attribute panel visibility */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    void ToggleAttributePanel();

    /** Get notification widget for showing messages */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    UNotificationWidget* GetNotificationWidget() const { return NotificationWidget; }

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

    /** Attribute panel widget class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UAttributePanelWidget> AttributePanelClass;

    /** Notification widget class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UNotificationWidget> NotificationWidgetClass;

private:
    UPROPERTY()
    TObjectPtr<UWorldSetupWidget> WorldSetupWidget;

    UPROPERTY()
    TObjectPtr<ULoadingWidget> LoadingWidget;

    UPROPERTY()
    TObjectPtr<UAttributePanelWidget> AttributePanel;

    UPROPERTY()
    TObjectPtr<UNotificationWidget> NotificationWidget;

    UPROPERTY()
    TObjectPtr<UChunkManagerComponent> ChunkManager;

    UPROPERTY()
    TObjectPtr<UPluginGameInstance> PluginGameInstance;
};
