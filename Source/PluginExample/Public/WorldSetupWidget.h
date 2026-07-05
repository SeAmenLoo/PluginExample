#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIStylingHelper.h"
#include "WorldSetupWidget.generated.h"

class UButton;
class UMultiLineEditableTextBox;
class UComboBoxString;
class UCheckBox;
class UBorder;
class UTextBlock;

UCLASS()
class PLUGINEXAMPLE_API UWorldSetupWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldSetupComplete, const FString&, WorldPrompt);

    UPROPERTY(BlueprintAssignable)
    FOnWorldSetupComplete OnWorldSetupComplete;

    /** Apply custom styling to the world setup widget */
    UFUNCTION(BlueprintCallable)
    void ApplyCustomStyling(const FUIStyleConfig& InStyleConfig);

    /** Style configuration for the world setup widget */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
    FUIStyleConfig StyleConfig;

    /** Background border widget (optional - can be added in Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BackgroundBorder;

    /** Title text widget (optional - can be added in Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UMultiLineEditableTextBox> WorldPromptTextBox;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UButton> StartGameButton;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UComboBoxString> GameStyleSelector;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UMultiLineEditableTextBox> PlayerCharacterNameBox;

    virtual void NativeConstruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION()
    void OnStartGameClicked();

    UFUNCTION()
    void OnStyleSelected(FString SelectedItem, ESelectInfo::Type SelectionType);
};
