#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIStylingHelper.h"
#include "LoadingWidget.generated.h"

class UTextBlock;
class UBorder;
class UImage;

UCLASS()
class PLUGINEXAMPLE_API ULoadingWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    ULoadingWidget(const FObjectInitializer& ObjectInitializer);

    /** Set the loading text to display */
    UFUNCTION(BlueprintCallable)
    void SetLoadingText(const FString& InLoadingText);

    /** Set the number of dots to animate (0 to stop) */
    UFUNCTION(BlueprintCallable)
    void SetAnimatedDots(bool bEnable);

    /** Apply custom styling to the loading widget */
    UFUNCTION(BlueprintCallable)
    void ApplyCustomStyling(const FUIStyleConfig& InStyleConfig);

    /** Style configuration for the loading widget */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
    FUIStyleConfig StyleConfig;

    /** Background border widget (optional - can be added in Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BackgroundBorder;

    /** Background image widget (optional - can be added in Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UImage> BackgroundImage;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> LoadingText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> DotsText;

private:
    void UpdateDotsText();

    FString BaseLoadingText;
    bool bAnimateDots = false;
    float DotTimer = 0.0f;
    int32 CurrentDotCount = 0;

    static constexpr float DotInterval = 0.5f;
    static constexpr int32 MaxDots = 3;
};
