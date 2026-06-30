#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldSetupWidget.generated.h"

class UButton;
class UEditableTextBox;
class UComboBoxString;
class UCheckBox;

UCLASS()
class PLUGINEXAMPLE_API UWorldSetupWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldSetupComplete, const FString&, WorldPrompt);

    UPROPERTY(BlueprintAssignable)
    FOnWorldSetupComplete OnWorldSetupComplete;

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UEditableTextBox> WorldPromptTextBox;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UButton> StartGameButton;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UComboBoxString> GameStyleSelector;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UEditableTextBox> PlayerCharacterNameBox;

    virtual void NativeConstruct() override;

    UFUNCTION()
    void OnStartGameClicked();

    UFUNCTION()
    void OnStyleSelected(FString SelectedItem, ESelectInfo::Type SelectionType);
};
