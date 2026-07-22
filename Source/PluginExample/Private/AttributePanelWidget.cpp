#include "AttributePanelWidget.h"
#include "AttributeRowWidget.h"
#include "AttributeSystemSubsystem.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UAttributePanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UAttributeSystemSubsystem* AttrSys = GI->GetSubsystem<UAttributeSystemSubsystem>())
        {
            AttrSys->OnAttributeChanged.AddDynamic(this, &UAttributePanelWidget::HandleAttributeChanged);
        }
    }
}

void UAttributePanelWidget::NativeDestruct()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UAttributeSystemSubsystem* AttrSys = GI->GetSubsystem<UAttributeSystemSubsystem>())
        {
            AttrSys->OnAttributeChanged.RemoveDynamic(this, &UAttributePanelWidget::HandleAttributeChanged);
        }
    }
    Super::NativeDestruct();
}

// ==================== Public API ====================

void UAttributePanelWidget::OpenPanel()
{
    bIsPanelOpen = true;
    RecentlyChangedAttributeIds.Empty();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    RefreshData();
}

void UAttributePanelWidget::ClosePanel()
{
    if (!bIsPanelOpen) return;
    bIsPanelOpen = false;
    SetVisibility(ESlateVisibility::Collapsed);
    OnPanelClosed.Broadcast();
}

void UAttributePanelWidget::TogglePanel()
{
    if (bIsPanelOpen) { ClosePanel(); } else { OpenPanel(); }
}

void UAttributePanelWidget::RefreshData()
{
    AttributeRows.Empty();

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UAttributeSystemSubsystem* AttrSys = GI->GetSubsystem<UAttributeSystemSubsystem>();
    if (!AttrSys) return;

    TArray<FPlayerAttribute> AllAttrs = AttrSys->GetAllAttributes();
    for (const FPlayerAttribute& Attr : AllAttrs)
    {
        AttributeRows.Add(BuildRowData(Attr));
    }

    // Preserve original LLM order, no type grouping
    RebuildRows();
}

// ==================== Helpers ====================

FString UAttributePanelWidget::FormatModifier(int32 Modifier)
{
    return UAttributeRowWidget::FormatModifier(Modifier);
}

// ==================== Private ====================

FAttributeRowData UAttributePanelWidget::BuildRowData(const FPlayerAttribute& Attr) const
{
    FAttributeRowData Row;
    Row.AttributeId = Attr.AttributeId;
    Row.DisplayName = Attr.DisplayName;
    Row.AttributeType = Attr.AttributeType;
    Row.CurrentValue = Attr.CurrentValue;
    Row.MinValue = Attr.MinValue;
    Row.MaxValue = Attr.MaxValue;
    Row.Modifier = FMath::FloorToInt((Attr.CurrentValue - 10) / 2.0f);
    Row.FillRatio = (Attr.MaxValue > Attr.MinValue)
        ? FMath::Clamp((float)(Attr.CurrentValue - Attr.MinValue) / (float)(Attr.MaxValue - Attr.MinValue), 0.0f, 1.0f)
        : 0.5f;
    Row.bRecentlyChanged = RecentlyChangedAttributeIds.Contains(Attr.AttributeId);
    return Row;
}

void UAttributePanelWidget::RebuildRows()
{
    if (!RootContainer) return;

    RootContainer->ClearChildren();
    RowWidgets.Empty();

    APlayerController* PC = GetOwningPlayer();

    for (const FAttributeRowData& Row : AttributeRows)
    {
        TSubclassOf<UAttributeRowWidget> WidgetClass = RowWidgetClass;
        if (!WidgetClass)
        {
            // Fallback to the C++ base class if no blueprint class is set
            WidgetClass = UAttributeRowWidget::StaticClass();
        }

        UAttributeRowWidget* RowWidget = CreateWidget<UAttributeRowWidget>(this, WidgetClass);
        if (!RowWidget) continue;

        RowWidget->SetupWithAttribute(Row);
        RowWidgets.Add(RowWidget);

        RootContainer->AddChild(RowWidget);
        if (auto* RowSlot = Cast<UVerticalBoxSlot>(RowWidget->Slot))
        {
            RowSlot->SetPadding(FMargin(0, RowSpacing / 2, 0, RowSpacing / 2));
        }
    }
}

void UAttributePanelWidget::HandleAttributeChanged(const FString& AttributeId, int32 OldValue, int32 NewValue)
{
    RecentlyChangedAttributeIds.Add(AttributeId);

    for (int32 i = 0; i < AttributeRows.Num(); ++i)
    {
        if (AttributeRows[i].AttributeId == AttributeId)
        {
            AttributeRows[i].CurrentValue = NewValue;
            AttributeRows[i].bRecentlyChanged = true;
            AttributeRows[i].Modifier = FMath::FloorToInt((NewValue - 10) / 2.0f);
            AttributeRows[i].FillRatio = (AttributeRows[i].MaxValue > AttributeRows[i].MinValue)
                ? FMath::Clamp((float)(NewValue - AttributeRows[i].MinValue) / (float)(AttributeRows[i].MaxValue - AttributeRows[i].MinValue), 0.0f, 1.0f)
                : 0.5f;

            if (RowWidgets.IsValidIndex(i))
            {
                RowWidgets[i]->UpdateValue(AttributeRows[i].CurrentValue, AttributeRows[i].MaxValue);
            }
            break;
        }
    }
}
