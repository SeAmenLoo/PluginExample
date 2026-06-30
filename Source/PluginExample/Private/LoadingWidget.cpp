#include "LoadingWidget.h"
#include "Components/TextBlock.h"

ULoadingWidget::ULoadingWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    BaseLoadingText = TEXT("Generating world");
    bAnimateDots = true;
}

void ULoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (LoadingText)
    {
        LoadingText->SetText(FText::FromString(BaseLoadingText));
        
        FSlateFontInfo FontInfo = LoadingText->GetFont();
        FontInfo.Size = 32;
        LoadingText->SetFont(FontInfo);
        
        LoadingText->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.95f, 1.0f));
        LoadingText->SetAutoWrapText(true);
    }

    if (DotsText)
    {
        FSlateFontInfo FontInfo = DotsText->GetFont();
        FontInfo.Size = 48;
        DotsText->SetFont(FontInfo);
        
        DotsText->SetColorAndOpacity(FLinearColor(0.3f, 0.7f, 1.0f, 1.0f));
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
