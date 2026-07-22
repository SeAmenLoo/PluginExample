#include "NotificationWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

void UNotificationWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UNotificationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    for (int32 i = ActiveWidgets.Num() - 1; i >= 0; --i)
    {
        FActiveNotificationWidget& Active = ActiveWidgets[i];
        Active.ElapsedTime += InDeltaTime;

        float TotalDuration = Active.Duration + FadeOutDuration;
        float Progress = FMath::Min(Active.ElapsedTime / TotalDuration, 1.0f);
        float VerticalOffset = Active.ElapsedTime * FloatSpeed;

        UpdateNotificationWidget(i, Progress, VerticalOffset);

        if (Active.ElapsedTime >= TotalDuration)
        {
            RemoveNotificationWidget(i);
            ActiveWidgets.RemoveAt(i);
        }
    }
}

// ==================== Public API ====================

void UNotificationWidget::ShowNotification(const FString& Text, ENotificationType Type)
{
    ShowNotificationEx(Text, Type, DefaultDuration);
}

void UNotificationWidget::ShowNotificationEx(const FString& Text, ENotificationType Type, float Duration)
{
    if (ActiveWidgets.Num() >= MaxNotifications)
    {
        RemoveNotificationWidget(0);
        ActiveWidgets.RemoveAt(0);
    }

    FNotificationEntry Entry;
    Entry.Text = Text;
    Entry.Type = Type;
    Entry.Duration = Duration;

    CreateNotificationWidget(Entry);
}

void UNotificationWidget::ShowAttributeChange(const FString& AttributeName, int32 OldValue, int32 NewValue)
{
    int32 Delta = NewValue - OldValue;
    FString Sign = Delta > 0 ? TEXT("+") : TEXT("");
    FString Text = FString::Printf(TEXT("%s: %d -> %d (%s%d)"),
        *AttributeName, OldValue, NewValue, *Sign, Delta);

    ENotificationType Type = Delta > 0 ? ENotificationType::Attribute : ENotificationType::Warning;
    ShowNotification(Text, Type);
}

void UNotificationWidget::ShowItemChange(const FString& ItemName, bool bAdded, int32 Quantity)
{
    FString Text = bAdded
        ? FString::Printf(TEXT("获得: %s x%d"), *ItemName, Quantity)
        : FString::Printf(TEXT("失去: %s x%d"), *ItemName, Quantity);
    ShowNotification(Text, ENotificationType::Item);
}

void UNotificationWidget::ShowError(const FString& ErrorText)
{
    ShowNotification(ErrorText, ENotificationType::Error);
}

void UNotificationWidget::ShowSuccess(const FString& SuccessText)
{
    ShowNotification(SuccessText, ENotificationType::Success);
}

void UNotificationWidget::ShowWarning(const FString& WarningText)
{
    ShowNotification(WarningText, ENotificationType::Warning);
}

void UNotificationWidget::ClearAllNotifications()
{
    for (int32 i = ActiveWidgets.Num() - 1; i >= 0; --i)
    {
        RemoveNotificationWidget(i);
    }
    ActiveWidgets.Empty();
}

// ==================== UI Creation ====================

void UNotificationWidget::CreateNotificationWidget(const FNotificationEntry& Entry)
{
    if (!NotificationCanvas) return;

    FLinearColor Color = GetNotificationColor(Entry.Type);

    // Border for background
    UBorder* Border = NewObject<UBorder>(this);
    Border->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, BackgroundOpacity));
    Border->SetPadding(FMargin(8, 6, 8, 6));

    // Text block
    UTextBlock* TextBlock = CreateStyledText(Entry.Text, 14.0f, Color);

    Border->AddChild(TextBlock);
    NotificationCanvas->AddChild(Border);

    // Position at bottom center
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Border->Slot))
    {
        CanvasSlot->SetAutoSize(true);
        CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
        CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));

        float StartY = -ActiveWidgets.Num() * NotificationSpacing;
        CanvasSlot->SetPosition(FVector2D(0.0f, StartY));
    }

    FActiveNotificationWidget Active;
    Active.Border = Border;
    Active.TextBlock = TextBlock;
    Active.ElapsedTime = 0.0f;
    Active.Duration = Entry.Duration;
    Active.StartY = -ActiveWidgets.Num() * NotificationSpacing;
    ActiveWidgets.Add(Active);
}

void UNotificationWidget::UpdateNotificationWidget(int32 Index, float Progress, float VerticalOffset)
{
    if (Index < 0 || Index >= ActiveWidgets.Num()) return;

    FActiveNotificationWidget& Active = ActiveWidgets[Index];

    if (!Active.Border || !Active.TextBlock) return;

    // Calculate opacity
    float Alpha = 1.0f;
    if (Progress < FadeInDuration / (Active.Duration + FadeOutDuration))
    {
        Alpha = Progress / (FadeInDuration / (Active.Duration + FadeOutDuration));
    }
    else if (Progress > Active.Duration / (Active.Duration + FadeOutDuration))
    {
        float FadeProgress = (Progress - Active.Duration / (Active.Duration + FadeOutDuration))
            / (FadeOutDuration / (Active.Duration + FadeOutDuration));
        Alpha = 1.0f - FadeProgress;
    }
    Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

    // Update text opacity
    FLinearColor TextColor = Active.TextBlock->ColorAndOpacity.GetSpecifiedColor();
    TextColor.A = Alpha;
    Active.TextBlock->SetColorAndOpacity(TextColor);

    // Update border opacity
    FLinearColor BorderColor = Active.Border->BrushColor;
    BorderColor.A = BackgroundOpacity * Alpha;
    Active.Border->SetBrushColor(BorderColor);

    // Update vertical position (float up)
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Active.Border->Slot))
    {
        float CurrentY = Active.StartY - VerticalOffset;
        CanvasSlot->SetPosition(FVector2D(0.0f, CurrentY));
    }
}

void UNotificationWidget::RemoveNotificationWidget(int32 Index)
{
    if (Index < 0 || Index >= ActiveWidgets.Num()) return;

    FActiveNotificationWidget& Active = ActiveWidgets[Index];
    if (Active.Border)
    {
        Active.Border->RemoveFromParent();
    }
}

UTextBlock* UNotificationWidget::CreateStyledText(const FString& Text, float FontSize, FLinearColor Color)
{
    UTextBlock* TextBlock = NewObject<UTextBlock>(this);
    TextBlock->SetText(FText::FromString(Text));
    TextBlock->SetColorAndOpacity(Color);

    FSlateFontInfo Font = TextBlock->GetFont();
    Font.Size = FontSize;
    TextBlock->SetFont(Font);
    TextBlock->SetAutoWrapText(true);

    return TextBlock;
}

// ==================== Protected ====================

FLinearColor UNotificationWidget::GetNotificationColor(ENotificationType Type) const
{
    switch (Type)
    {
        case ENotificationType::Info:      return InfoColor;
        case ENotificationType::Success:   return SuccessColor;
        case ENotificationType::Warning:   return WarningColor;
        case ENotificationType::Error:     return ErrorColor;
        case ENotificationType::Attribute: return AttributeUpColor;
        case ENotificationType::Item:      return ItemColor;
        default:                           return InfoColor;
    }
}