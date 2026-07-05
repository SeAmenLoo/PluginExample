#include "LoadingWidget.h"
#include "UIStylingHelper.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"

ULoadingWidget::ULoadingWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    BaseLoadingText = TEXT("Generating world");
    bAnimateDots = true;
}

void ULoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Apply custom styling
    ApplyCustomStyling(StyleConfig);

    if (LoadingText)
    {
        LoadingText->SetText(FText::FromString(BaseLoadingText));
    }

    UpdateDotsText();
}

void ULoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bAnimateDots)
    {
        DotTimer += InDeltaTime;
        if (DotTimer >= DotInterval)
        {
            DotTimer = 0.0f;
            CurrentDotCount = (CurrentDotCount + 1) % (MaxDots + 1);
            UpdateDotsText();
        }
    }
}

void ULoadingWidget::SetLoadingText(const FString& InLoadingText)
{
    BaseLoadingText = InLoadingText;
    if (LoadingText)
    {
        LoadingText->SetText(FText::FromString(BaseLoadingText));
    }
}

void ULoadingWidget::SetAnimatedDots(bool bEnable)
{
    bAnimateDots = bEnable;
    if (!bEnable)
    {
        CurrentDotCount = 0;
        DotTimer = 0.0f;
    }
    UpdateDotsText();
}

void ULoadingWidget::ApplyCustomStyling(const FUIStyleConfig& InStyleConfig)
{
    StyleConfig = InStyleConfig;

    // Style loading text
    if (LoadingText)
    {
        UUIStylingHelper::StyleTextBlock(LoadingText, StyleConfig, true);
        FSlateFontInfo FontInfo = LoadingText->GetFont();
        FontInfo.Size = 32;
        LoadingText->SetFont(FontInfo);
    }

    // Style dots text
    if (DotsText)
    {
        FSlateFontInfo FontInfo = DotsText->GetFont();
        FontInfo.Size = 48;
        DotsText->SetFont(FontInfo);
        DotsText->SetColorAndOpacity(FLinearColor(0.3f, 0.7f, 1.0f, 1.0f));
    }

    // Apply background styling if available
    if (BackgroundBorder)
    {
        BackgroundBorder->SetBrushColor(StyleConfig.BackgroundColor);
        BackgroundBorder->SetPadding(FMargin(StyleConfig.Padding.X * 2, StyleConfig.Padding.Y * 2));
    }
}

void ULoadingWidget::UpdateDotsText()
{
    if (!DotsText)
    {
        return;
    }

    FString DotsStr;
    for (int32 i = 0; i < CurrentDotCount; ++i)
    {
        DotsStr += TEXT(".");
    }

    DotsText->SetText(FText::FromString(DotsStr));
}
