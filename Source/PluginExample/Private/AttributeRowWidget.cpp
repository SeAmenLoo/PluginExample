#include "AttributeRowWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"

void UAttributeRowWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UAttributeRowWidget::SetupWithAttribute(const FAttributeRowData& RowData)
{
    CachedRowData = RowData;

    if (!bLayoutBuilt)
    {
        BuildDefaultLayout();
        bLayoutBuilt = true;
    }

    RefreshDisplay();
    OnSetupWithAttribute(RowData);
}

void UAttributeRowWidget::UpdateValue(int32 NewValue, int32 NewMax)
{
    CachedRowData.CurrentValue = NewValue;
    CachedRowData.MaxValue = FMath::Max(1, NewMax);
    CachedRowData.Modifier = FMath::FloorToInt((CachedRowData.CurrentValue - 10) / 2.0f);
    CachedRowData.FillRatio = FMath::Clamp(
        (float)(CachedRowData.CurrentValue - CachedRowData.MinValue)
        / (float)(CachedRowData.MaxValue - CachedRowData.MinValue),
        0.0f, 1.0f);

    RefreshDisplay();
    OnValueUpdated(
        CachedRowData.CurrentValue,
        CachedRowData.MaxValue,
        CachedRowData.Modifier,
        CachedRowData.FillRatio);
}

// ==================== Helpers ====================

FString UAttributeRowWidget::FormatModifier(int32 Modifier)
{
    return (Modifier > 0) ? FString::Printf(TEXT("+%d"), Modifier) : FString::Printf(TEXT("%d"), Modifier);
}

FLinearColor UAttributeRowWidget::GetProgressBarColor(int32 Value) const
{
    if (Value >= 16) return BarHighColor;
    if (Value <= 5)  return BarLowColor;
    return BarFillColor;
}

// ==================== Default Layout ====================

void UAttributeRowWidget::BuildDefaultLayout()
{
    // Background border
    BackgroundBorder = NewObject<UBorder>(this);
    BackgroundBorder->SetPadding(RowPadding);

    // Main vertical layout
    UVerticalBox* MainVertical = NewObject<UVerticalBox>(this);

    // Top horizontal line: [Name] [Value] [Modifier]
    UHorizontalBox* TopLine = NewObject<UHorizontalBox>(this);

    // Name
    NameText = NewObject<UTextBlock>(this);
    NameText->SetColorAndOpacity(NameColor);
    FSlateFontInfo NameFont = NameText->GetFont();
    NameFont.Size = NameFontSize;
    NameText->SetFont(NameFont);
    TopLine->AddChild(NameText);
    if (auto* LocalNameSlot = Cast<UHorizontalBoxSlot>(NameText->Slot))
    {
        LocalNameSlot->SetSize(ESlateSizeRule::Fill);
        LocalNameSlot->SetPadding(FMargin(0, 0, 8, 0));
    }

    // Spacer between name and value
    USizeBox* Spacer = NewObject<USizeBox>(this);
    Spacer->SetWidthOverride(8.0f);
    TopLine->AddChild(Spacer);
    if (auto* LocalSpacerSlot = Cast<UHorizontalBoxSlot>(Spacer->Slot))
    {
        LocalSpacerSlot->SetSize(ESlateSizeRule::Automatic);
    }

    // Value
    ValueText = NewObject<UTextBlock>(this);
    ValueText->SetColorAndOpacity(ValueColor);
    FSlateFontInfo ValueFont = ValueText->GetFont();
    ValueFont.Size = ValueFontSize;
    ValueText->SetFont(ValueFont);
    TopLine->AddChild(ValueText);
    if (auto* LocalValueSlot = Cast<UHorizontalBoxSlot>(ValueText->Slot))
    {
        LocalValueSlot->SetSize(ESlateSizeRule::Fill);
        LocalValueSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Right);
    }

    // Modifier
    ModifierText = NewObject<UTextBlock>(this);
    ModifierText->SetColorAndOpacity(ModifierPositiveColor);
    FSlateFontInfo ModFont = ModifierText->GetFont();
    ModFont.Size = ModifierFontSize;
    ModifierText->SetFont(ModFont);
    TopLine->AddChild(ModifierText);
    if (auto* LocalModSlot = Cast<UHorizontalBoxSlot>(ModifierText->Slot))
    {
        LocalModSlot->SetSize(ESlateSizeRule::Automatic);
        LocalModSlot->SetPadding(FMargin(8, 0, 0, 0));
        LocalModSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Right);
    }

    MainVertical->AddChild(TopLine);

    // Progress bar
    ProgressBarWidget = NewObject<UProgressBar>(this);
    USizeBox* BarSize = NewObject<USizeBox>(this);
    BarSize->SetHeightOverride(ProgressBarHeight);
    BarSize->AddChild(ProgressBarWidget);
    MainVertical->AddChild(BarSize);
    if (auto* LocalBarSlot = Cast<UVerticalBoxSlot>(BarSize->Slot))
    {
        LocalBarSlot->SetPadding(FMargin(0, 4, 0, 0));
    }

    BackgroundBorder->AddChild(MainVertical);
    WidgetTree->RootWidget = BackgroundBorder;
}

void UAttributeRowWidget::RefreshDisplay()
{
    if (!bLayoutBuilt) return;

    if (NameText)
    {
        NameText->SetText(FText::FromString(CachedRowData.DisplayName));
    }

    if (ValueText)
    {
        ValueText->SetText(FText::FromString(
            FString::Printf(TEXT("%d/%d"), CachedRowData.CurrentValue, CachedRowData.MaxValue)));
    }

    if (ModifierText)
    {
        FString ModStr = FormatModifier(CachedRowData.Modifier);
        ModifierText->SetText(FText::FromString(ModStr));
        FLinearColor ModColor = CachedRowData.Modifier > 0 ? ModifierPositiveColor
            : (CachedRowData.Modifier < 0 ? ModifierNegativeColor : NameColor);
        ModifierText->SetColorAndOpacity(ModColor);
    }

    if (ProgressBarWidget)
    {
        ProgressBarWidget->SetPercent(CachedRowData.FillRatio);
        ProgressBarWidget->SetFillColorAndOpacity(GetProgressBarColor(CachedRowData.CurrentValue));
    }

    if (BackgroundBorder)
    {
        BackgroundBorder->SetBrushColor(
            CachedRowData.bRecentlyChanged ? ChangedBackgroundColor : BackgroundColor);
    }
}
