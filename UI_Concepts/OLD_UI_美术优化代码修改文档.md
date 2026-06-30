# UI 美术向优化代码修改文档

> 本文档详细说明如何修改现有 C++ 代码和 UMG 蓝图，实现三个核心 Widget（DialogueWidget、WorldSetupWidget、LoadingWidget）的美术向美化优化。
>
> 配套文档：[UI 风格动态替换逻辑文档](./UI_风格动态替换逻辑文档.md)

---

## 目录

1. [整体架构建议](#1-整体架构建议)
2. [DialogueWidget 美化修改](#2-dialoguewidget-美化修改)
3. [WorldSetupWidget 美化修改](#3-worldsetupwidget-美化修改)
4. [LoadingWidget 美化修改](#4-loadingwidget-美化修改)
5. [通用样式应用方法](#5-通用样式应用方法)

---

## 1. 整体架构建议

### 1.1 引入 UUIStyleData 数据资产

建议创建 `UUIStyleData`（继承自 `UDataAsset`）来统一管理所有 UI 颜色、字体、间距、动画参数。每个世界观风格对应一个 DataAsset 实例。这样可以实现一处修改、全局生效，同时为风格动态切换提供基础。

详细的 DataAsset 结构体设计和子系统实现请参见 [UI 风格动态替换逻辑文档](./UI_风格动态替换逻辑文档.md)。

### 1.2 创建 UBaseStyledWidget 基类

建议创建 `UBaseStyledWidget` 基类，封装通用的样式应用逻辑，所有需要风格支持的 Widget 都继承自该基类：

```cpp
// BaseStyledWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BaseStyledWidget.generated.h"

class UUIStyleData;

UCLASS(Abstract)
class PLUGINEXAMPLE_API UBaseStyledWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Apply the given style to this widget and all child widgets */
    UFUNCTION(BlueprintCallable, Category = "UI Style")
    virtual void ApplyStyle(UUIStyleData* Style);

protected:
    virtual void NativeConstruct() override;

    /** Blueprint implementable event for style application */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI Style")
    void OnStyleApplied(UUIStyleData* Style);

    /** Get the current UI style from the style manager subsystem */
    UFUNCTION(BlueprintCallable, Category = "UI Style")
    UUIStyleData* GetCurrentStyle() const;

    /** Helper: apply color and opacity to a text block */
    static void ApplyTextStyle(UTextBlock* TextBlock, const FLinearColor& Color, int32 FontSize);

    /** Helper: apply background brush to a border */
    static void ApplyBorderStyle(UBorder* BorderWidget, const FLinearColor& BackgroundColor,
                                  const FLinearColor& BorderColor, const FVector2D& CornerRadii);
};
```

```cpp
// BaseStyledWidget.cpp
#include "BaseStyledWidget.h"
#include "UIStyleData.h"
#include "UIStyleManagerSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"

void UBaseStyledWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Auto-apply current style on construct
    if (UUIStyleData* CurrentStyle = GetCurrentStyle())
    {
        ApplyStyle(CurrentStyle);
    }
}

UUIStyleData* UBaseStyledWidget::GetCurrentStyle() const
{
    UGameInstance* GameInst = GetGameInstance();
    if (!GameInst)
    {
        return nullptr;
    }

    UUIStyleManagerSubsystem* StyleManager = GameInst->GetSubsystem<UUIStyleManagerSubsystem>();
    if (StyleManager)
    {
        return StyleManager->GetCurrentStyle();
    }
    return nullptr;
}

void UBaseStyledWidget::ApplyStyle(UUIStyleData* Style)
{
    OnStyleApplied(Style);
}

void UBaseStyledWidget::ApplyTextStyle(
    UTextBlock* TextBlock,
    const FLinearColor& Color,
    int32 FontSize)
{
    if (!TextBlock)
    {
        return;
    }
    TextBlock->SetColorAndOpacity(Color);
    FSlateFontInfo FontInfo = TextBlock->GetFont();
    FontInfo.Size = FontSize;
    TextBlock->SetFont(FontInfo);
}

void UBaseStyledWidget::ApplyBorderStyle(
    UBorder* BorderWidget,
    const FLinearColor& BackgroundColor,
    const FLinearColor& BorderColor,
    const FVector2D& CornerRadii)
{
    if (!BorderWidget)
    {
        return;
    }
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FLinearColor(BackgroundColor.R, BackgroundColor.G, BackgroundColor.B, BackgroundColor.A);
    Brush.OutlineSettings.CornerRadii = CornerRadii;
    Brush.OutlineSettings.Color = BorderColor;
    Brush.OutlineSettings.Width = 2.0f;
    BorderWidget->SetBrush(Brush);
}
```

### 1.3 使用 UMG 控件模板

在 UMG Editor 中，可以通过以下方式复用样式：

1. **创建 Widget Blueprint 模板**：将通用控件组合（如带样式的按钮、带背景的面板）保存为独立的 Widget Blueprint
2. **通过 `Child Widget` 引用**：在父 Widget 中添加 `Child Widget` 引用模板类
3. **使用 `Widget Switcher`**：在同一位置放置不同风格版本的 Widget，通过代码切换

---

## 2. DialogueWidget 美化修改

当前 `UDialogueWidget` 的主要问题：
- NPC 名称使用纯 `UTextBlock`，无装饰效果
- 消息历史使用简单的 `UVerticalBox` + `UTextBlock`，无气泡效果
- 选项按钮是裸 `UButton`，无视觉反馈
- 思考中指示器只是跳动点文本，视觉效果弱
- 输入框和发送按钮缺乏样式处理

### 2.1 背景与容器

**目标**：添加半透明背景、圆角边框、磨砂玻璃效果。

**新增 BindWidget 属性**：

```cpp
// DialogueWidget.h - 在原有类中新增以下属性

class UImage;
class UBorder;
class UOverlay;

// ... 在 UDialogueWidget 类内新增 ...

/** Background overlay image for blur/dim effect */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> BackgroundOverlayImage;

/** Main border container with rounded corners */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UBorder> MainBorder;

/** Root overlay that holds background + content */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UOverlay> RootOverlay;
```

**NativeConstruct 中设置背景样式**：

```cpp
// DialogueWidget.cpp - NativeConstruct 中新增

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/Border.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"

void UDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // ... 原有按钮绑定代码保持不变 ...

    // Setup frosted glass background
    SetupBackgroundOverlay();

    // ... 其余原有代码 ...
}

void UDialogueWidget::SetupBackgroundOverlay()
{
    // Set semi-transparent background overlay
    if (BackgroundOverlayImage)
    {
        BackgroundOverlayImage->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
    }

    // Setup rounded border container
    if (MainBorder)
    {
        FSlateBrush BorderBrush;
        BorderBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        BorderBrush.TintColor = FLinearColor(0.1f, 0.1f, 0.15f, 0.85f);
        BorderBrush.OutlineSettings.CornerRadii = FVector2D(16.0f, 16.0f);
        BorderBrush.OutlineSettings.Color = FLinearColor(0.3f, 0.3f, 0.4f, 0.8f);
        BorderBrush.OutlineSettings.Width = 2.0f;
        MainBorder->SetBrush(BorderBrush);
    }
}
```

**头文件中新增辅助函数声明**：

```cpp
// DialogueWidget.h - private 区域新增
private:
    void SetupBackgroundOverlay();
    void ApplyNPCNameStyle();
    void ApplyDialogueTextStyle();
    void ApplyInputFieldStyle();
    UDialogueMessageEntry* CreateMessageEntryWidget(
        const FString& SpeakerName, const FString& Message, bool bIsPlayer);
    void StyleChoiceButton(UButton* Button, int32 Index, int32 Total);
```

### 2.2 NPC 名称美化

**目标**：使用 `URichTextBlock` 支持富文本，添加阴影和装饰效果。

> **注意**：当前代码中 `NPCNameText` 为 `UTextBlock*`。如果需要支持富文本，可在蓝图中替换为 `URichTextBlock`，C++ 端保留 `UTextBlock` 以保持兼容性。若要使用 `URichTextBlock`，可在蓝图中添加并单独绑定。

**修改后的 NPC 名称样式设置**：

```cpp
// DialogueWidget.cpp
#include "Components/RichTextBlock.h"
#include "Components/RichTextBlock.h"
#include "Styling/CoreEnums.h"

void UDialogueWidget::ApplyNPCNameStyle()
{
    if (NPCNameText)
    {
        FSlateFontInfo NameFont = NPCNameText->GetFont();
        NameFont.Size = 26;
        NameFont.OutlineSettings.OutlineSize = 2;
        NameFont.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
        NPCNameText->SetFont(NameFont);
        NPCNameText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.5f, 1.0f));

        // Shadow effect for depth
        NPCNameText->SetShadowOffset(FVector2D(1.5f, 1.5f));
        NPCNameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
    }
}
```

**NativeConstruct 中调用**：

```cpp
void UDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // ... 原有按钮绑定 ...

    SetupBackgroundOverlay();
    ApplyNPCNameStyle();
    ApplyDialogueTextStyle();
    ApplyInputFieldStyle();

    // Hide input initially
    HideInputField();
}
```

### 2.3 对话气泡

**目标**：将消息历史从纯文本改为气泡结构，区分玩家/NPC 气泡样式。

**新增 DialogueMessageEntry 子控件**：

```cpp
// DialogueMessageEntry.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DialogueMessageEntry.generated.h"

class UTextBlock;
class UBorder;
class UImage;
class USizeBox;
class UHorizontalBox;

UCLASS()
class PLUGINEXAMPLE_API UDialogueMessageEntry : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Initialize the message entry with data */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SetupMessage(const FString& SpeakerName, const FString& Message,
                      bool bIsPlayer, const FString& Timestamp);

    /** Apply bubble style for player or NPC */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void ApplyBubbleStyle(bool bIsPlayer);

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SpeakerNameText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> MessageText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BubbleBorder;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UImage> AvatarImage;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TimestampText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> MessageContainer;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY()
    FString SpeakerName;

    UPROPERTY()
    bool bIsPlayerMessage = false;
};
```

```cpp
// DialogueMessageEntry.cpp
#include "DialogueMessageEntry.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"

void UDialogueMessageEntry::NativeConstruct()
{
    Super::NativeConstruct();
}

void UDialogueMessageEntry::SetupMessage(
    const FString& InSpeakerName,
    const FString& InMessage,
    bool bInIsPlayer,
    const FString& InTimestamp)
{
    SpeakerName = InSpeakerName;
    bIsPlayerMessage = bInIsPlayer;

    if (SpeakerNameText)
    {
        SpeakerNameText->SetText(FText::FromString(InSpeakerName));
    }
    if (MessageText)
    {
        MessageText->SetText(FText::FromString(InMessage));
    }
    if (TimestampText && !InTimestamp.IsEmpty())
    {
        TimestampText->SetText(FText::FromString(InTimestamp));
    }

    ApplyBubbleStyle(bInIsPlayer);
}

void UDialogueMessageEntry::ApplyBubbleStyle(bool bIsPlayer)
{
    // Apply speaker name styling
    if (SpeakerNameText)
    {
        if (bIsPlayer)
        {
            SpeakerNameText->SetColorAndOpacity(FLinearColor(0.3f, 0.8f, 1.0f, 1.0f));
        }
        else
        {
            SpeakerNameText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.5f, 1.0f));
        }
        FSlateFontInfo NameFont = SpeakerNameText->GetFont();
        NameFont.Size = 14;
        SpeakerNameText->SetFont(NameFont);
    }

    // Apply message text styling
    if (MessageText)
    {
        MessageText->SetColorAndOpacity(FLinearColor(0.95f, 0.95f, 1.0f, 1.0f));
        FSlateFontInfo MsgFont = MessageText->GetFont();
        MsgFont.Size = 15;
        MessageText->SetFont(MsgFont);
        MessageText->SetAutoWrapText(true);
    }

    // Apply bubble border styling
    if (BubbleBorder)
    {
        FSlateBrush BubbleBrush;
        BubbleBrush.DrawAs = ESlateBrushDrawType::RoundedBox;

        if (bIsPlayer)
        {
            // Player bubble: blue tones
            BubbleBrush.TintColor = FLinearColor(0.15f, 0.3f, 0.5f, 0.9f);
            BubbleBrush.OutlineSettings.Color = FLinearColor(0.3f, 0.6f, 1.0f, 0.8f);
            if (MessageContainer)
            {
                MessageContainer->SetHorizontalAlignment(HAlign_Right);
            }
        }
        else
        {
            // NPC bubble: warm tones
            BubbleBrush.TintColor = FLinearColor(0.3f, 0.2f, 0.1f, 0.9f);
            BubbleBrush.OutlineSettings.Color = FLinearColor(0.6f, 0.5f, 0.3f, 0.8f);
            if (MessageContainer)
            {
                MessageContainer->SetHorizontalAlignment(HAlign_Left);
            }
        }

        BubbleBrush.OutlineSettings.CornerRadii = FVector2D(12.0f, 12.0f);
        BubbleBrush.OutlineSettings.Width = 1.5f;
        BubbleBorder->SetBrush(BubbleBrush);
    }

    // Apply timestamp styling
    if (TimestampText)
    {
        TimestampText->SetColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.65f, 0.7f));
        FSlateFontInfo TimeFont = TimestampText->GetFont();
        TimeFont.Size = 10;
        TimestampText->SetFont(TimeFont);
    }
}
```

**修改 AddMessageToHistory 使用子控件**：

```cpp
// DialogueWidget.h - 新增属性
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
TSubclassOf<UDialogueMessageEntry> MessageEntryWidgetClass;

// DialogueWidget.cpp - 修改 AddMessageToHistory
#include "DialogueMessageEntry.h"

void UDialogueWidget::AddMessageToHistory(
    const FString& SpeakerName,
    const FString& Message,
    bool bIsPlayer)
{
    if (!HistoryContainer || !MessageEntryWidgetClass)
    {
        return;
    }

    // Create message entry widget
    UDialogueMessageEntry* Entry = CreateWidget<UDialogueMessageEntry>(this, MessageEntryWidgetClass);

    // Get current timestamp
    FDateTime Now = FDateTime::Now();
    FString Timestamp = Now.ToString(TEXT("HH:mm"));

    Entry->SetupMessage(SpeakerName, Message, bIsPlayer, Timestamp);

    HistoryContainer->AddChildToVerticalBox(Entry);

    // Scroll to bottom
    if (HistoryScrollBox)
    {
        HistoryScrollBox->ScrollToEnd();
    }

    OnMessageAdded();
}
```

### 2.4 选项按钮美化

**目标**：从裸 `UButton` 改为带序号的选项卡片，添加 Hover 动画和选中高亮。

**新增 BindWidget**：

```cpp
// DialogueWidget.h - 新增属性

class UOverlay;
class UTextBlock;

/** Style data for choice button backgrounds */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UOverlay> ChoiceButtonOverlay;
```

**修改 ShowChoices 方法**：

```cpp
// DialogueWidget.cpp - 修改 ShowChoices
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Styling/SlateBrush.h"

void UDialogueWidget::ShowChoices(const TArray<FString>& Choices)
{
    if (!ChoicesBox || !ChoicesContainer)
    {
        return;
    }

    ChoicesBox->ClearChildren();
    ChoicesContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    for (int32 i = 0; i < Choices.Num(); ++i)
    {
        // Create a horizontal box for each choice (index + text)
        UHorizontalBox* ChoiceRow = NewObject<UHorizontalBox>(ChoicesBox);

        // Index text
        UTextBlock* IndexText = NewObject<UTextBlock>(ChoiceRow);
        FString IndexStr = FString::Printf(TEXT("  %d.  "), i + 1);
        IndexText->SetText(FText::FromString(IndexStr));
        IndexText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.5f, 1.0f));
        FSlateFontInfo IndexFont = IndexText->GetFont();
        IndexFont.Size = 18;
        IndexFont.OutlineSettings.OutlineSize = 1;
        IndexFont.OutlineSettings.OutlineColor = FLinearColor::Black;
        IndexText->SetFont(IndexFont);
        ChoiceRow->AddChild(IndexText);

        // Choice button with styled text
        UButton* ChoiceButton = NewObject<UButton>(ChoiceRow);

        // Style the button background
        FSlateBrush ButtonBrush;
        ButtonBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        ButtonBrush.TintColor = FLinearColor(0.2f, 0.15f, 0.1f, 0.9f);
        ButtonBrush.OutlineSettings.CornerRadii = FVector2D(8.0f, 8.0f);
        ButtonBrush.OutlineSettings.Color = FLinearColor(0.5f, 0.4f, 0.2f, 0.8f);
        ButtonBrush.OutlineSettings.Width = 1.0f;
        ChoiceButton->SetBackgroundBrush(ButtonBrush);

        // Hover style
        FSlateBrush HoverBrush;
        HoverBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        HoverBrush.TintColor = FLinearColor(0.3f, 0.25f, 0.15f, 0.95f);
        HoverBrush.OutlineSettings.CornerRadii = FVector2D(8.0f, 8.0f);
        HoverBrush.OutlineSettings.Color = FLinearColor(1.0f, 0.85f, 0.5f, 1.0f);
        HoverBrush.OutlineSettings.Width = 2.0f;
        ChoiceButton->SetHoverBrush(HoverBrush);

        UTextBlock* ChoiceText = NewObject<UTextBlock>(ChoiceButton);
        ChoiceText->SetText(FText::FromString(Choices[i]));
        ChoiceText->SetColorAndOpacity(FLinearColor(0.95f, 0.95f, 1.0f, 1.0f));
        ChoiceText->SetAutoWrapText(true);
        FSlateFontInfo ChoiceFont = ChoiceText->GetFont();
        ChoiceFont.Size = 16;
        ChoiceText->SetFont(ChoiceFont);

        // Set padding for the button content
        ChoiceButton->SetPadding(FMargin(16.0f, 10.0f, 16.0f, 10.0f));

        ChoiceButton->AddChild(ChoiceText);

        ChoiceRow->AddChild(ChoiceButton);
        ChoicesBox->AddChild(ChoiceRow);

        // Bind click event using Slate delegate (already correct in existing code)
        const int32 Index = i;
        TSharedRef<SWidget> WidgetRef = ChoiceButton->TakeWidget();
        TSharedRef<SButton> SlateButtonRef = StaticCastSharedRef<SButton>(WidgetRef);
        SlateButtonRef->SetOnClicked(FOnClicked::CreateWeakLambda(this, [this, Index]() -> FReply
        {
            SelectChoice(Index);
            return FReply::Handled();
        }));
    }

    // Add spacing between choice rows
    if (ChoicesBox)
    {
        UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(
            ChoicesBox->AddChildToVerticalBox(NewObject<USpacer>(ChoicesBox)));
        if (BoxSlot)
        {
            BoxSlot->SetAutoSize();
            BoxSlot->SetPadding(FMargin(0.0f, 8.0f));
        }
    }

    ShowInputField();

    if (ContinueButton)
    {
        ContinueButton->SetVisibility(ESlateVisibility::Collapsed);
    }
}
```

### 2.5 输入区域美化

**目标**：美化 `UEditableTextBox` 的背景、边框、焦点态，发送按钮图标化。

**新增 BindWidget**：

```cpp
// DialogueWidget.h - 新增属性

class UBorder;

/** Input area border for background styling */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UBorder> InputAreaBorder;

/** Send button icon image */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> SendButtonIconImage;
```

**NativeConstruct 中的输入框样式设置**：

```cpp
// DialogueWidget.cpp

void UDialogueWidget::ApplyInputFieldStyle()
{
    // Style the input area border
    if (InputAreaBorder)
    {
        FSlateBrush InputBorderBrush;
        InputBorderBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        InputBorderBrush.TintColor = FLinearColor(0.12f, 0.12f, 0.18f, 0.95f);
        InputBorderBrush.OutlineSettings.CornerRadii = FVector2D(10.0f, 10.0f);
        InputBorderBrush.OutlineSettings.Color = FLinearColor(0.3f, 0.3f, 0.45f, 0.9f);
        InputBorderBrush.OutlineSettings.Width = 1.5f;
        InputAreaBorder->SetBrush(InputBorderBrush);
    }

    // Style the editable text box
    if (InputTextBox)
    {
        FSlateBrush BackgroundBrush;
        BackgroundBrush.TintColor = FLinearColor(0.08f, 0.08f, 0.12f, 1.0f);
        InputTextBox->SetBackgroundBrush(BackgroundBrush);

        FSlateBrush ForegroundBrush;
        ForegroundBrush.TintColor = FLinearColor(0.4f, 0.4f, 0.55f, 1.0f);
        InputTextBox->SetForegroundBrush(ForegroundBrush);

        FSlateFontInfo InputFont = InputTextBox->GetFont();
        InputFont.Size = 16;
        InputTextBox->SetFont(InputFont);
        InputTextBox->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.95f, 1.0f));

        // Set padding for comfortable text area
        InputTextBox->SetPadding(FMargin(12.0f, 8.0f));
    }

    // Style the send button
    if (SendButton)
    {
        FSlateBrush NormalBrush;
        NormalBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        NormalBrush.TintColor = FLinearColor(0.25f, 0.2f, 0.15f, 1.0f);
        NormalBrush.OutlineSettings.CornerRadii = FVector2D(8.0f, 8.0f);
        NormalBrush.OutlineSettings.Color = FLinearColor(0.6f, 0.5f, 0.3f, 1.0f);
        NormalBrush.OutlineSettings.Width = 1.5f;
        SendButton->SetBackgroundBrush(NormalBrush);

        FSlateBrush HoverBrush;
        HoverBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        HoverBrush.TintColor = FLinearColor(0.35f, 0.28f, 0.18f, 1.0f);
        HoverBrush.OutlineSettings.CornerRadii = FVector2D(8.0f, 8.0f);
        HoverBrush.OutlineSettings.Color = FLinearColor(1.0f, 0.85f, 0.5f, 1.0f);
        HoverBrush.OutlineSettings.Width = 2.0f;
        SendButton->SetHoverBrush(HoverBrush);

        SendButton->SetPadding(FMargin(10.0f, 8.0f));
    }
}
```

### 2.6 思考中指示器美化

**目标**：使用 `UCircularThrobber` 替代纯文本跳动点。

**新增 BindWidget**：

```cpp
// DialogueWidget.h - 新增属性

class UCircularThrobber;
class UOverlay;

/** Animated throbber for thinking state */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UCircularThrobber> ThinkingThrobber;

/** Container for thinking indicator */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UOverlay> ThinkingIndicatorContainer;
```

**修改思考指示器的显示/隐藏逻辑**：

```cpp
// DialogueWidget.cpp
#include "Components/CircularThrobber.h"

void UDialogueWidget::ShowThinkingIndicator()
{
    bThinkingVisible = true;
    ThinkingDotTimer = 0.0f;
    ThinkingDotCount = 0;

    // Show the animated throbber
    if (ThinkingThrobber)
    {
        ThinkingThrobber->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    // Also keep the text as fallback
    if (ThinkingText)
    {
        ThinkingText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ThinkingText->SetText(FText::FromString(TEXT("思考中")));
    }

    // Show the whole container
    if (ThinkingIndicatorContainer)
    {
        ThinkingIndicatorContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    HideInputField();

    if (ChoicesContainer)
    {
        ChoicesContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDialogueWidget::HideThinkingIndicator()
{
    bThinkingVisible = false;
    ThinkingDotTimer = 0.0f;
    ThinkingDotCount = 0;

    if (ThinkingThrobber)
    {
        ThinkingThrobber->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (ThinkingText)
    {
        ThinkingText->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (ThinkingIndicatorContainer)
    {
        ThinkingIndicatorContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
}
```

> **蓝图中的操作**：在 Widget Blueprint 中，将原来的 `ThinkingText` 保留为备选显示，新增 `UCircularThrobber` 控件命名为 `ThinkingThrobber`。可以将 Throbber 放在 ThinkingText 左侧，形成一个横向排列的指示器区域。

---

## 3. WorldSetupWidget 美化修改

当前 `UWorldSetupWidget` 的主要问题：
- 无背景和装饰元素
- 使用默认 `UComboBoxString`，视觉表现力弱
- 输入框无统一样式
- 开始按钮缺乏视觉冲击力

### 3.1 整体布局

**目标**：添加背景图、居中卡片容器、装饰边框。

**新增 BindWidget**：

```cpp
// WorldSetupWidget.h - 新增属性

class UImage;
class UBorder;
class UOverlay;
class UTextBlock;
class UVerticalBox;

/** Full-screen background image */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> BackgroundImage;

/** Center card container border */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UBorder> CardBorder;

/** Card content overlay */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UOverlay> CardOverlay;

/** Page title text */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> TitleText;

/** Left decorative line image */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> LeftDecoLine;

/** Right decorative line image */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> RightDecoLine;

/** Main content vertical box */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UVerticalBox> ContentBox;
```

**NativeConstruct 中设置布局样式**：

```cpp
// WorldSetupWidget.cpp
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"

void UWorldSetupWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Apply layout styles
    ApplyLayoutStyle();

    // Bind button events
    if (StartGameButton)
    {
        StartGameButton->OnClicked.AddDynamic(this, &UWorldSetupWidget::OnStartGameClicked);
    }

    // Populate game style options (keep existing logic)
    if (GameStyleSelector)
    {
        GameStyleSelector->AddOption(TEXT("Medieval Fantasy"));
        GameStyleSelector->AddOption(TEXT("Sci-Fi Future"));
        GameStyleSelector->AddOption(TEXT("Modern Horror"));
        GameStyleSelector->AddOption(TEXT("Post-Apocalyptic"));
        GameStyleSelector->AddOption(TEXT("Custom"));
        GameStyleSelector->SetSelectedOption(TEXT("Medieval Fantasy"));
    }

    // Apply input field styles
    ApplyInputFieldStyles();
    ApplyStartButtonStyle();
    ApplyTitleStyle();
}

void UWorldSetupWidget::ApplyLayoutStyle()
{
    // Background image - full screen
    if (BackgroundImage)
    {
        BackgroundImage->SetColorAndOpacity(FLinearColor(0.15f, 0.12f, 0.2f, 0.9f));
    }

    // Card container - centered with rounded corners
    if (CardBorder)
    {
        FSlateBrush CardBrush;
        CardBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        CardBrush.TintColor = FLinearColor(0.08f, 0.08f, 0.12f, 0.95f);
        CardBrush.OutlineSettings.CornerRadii = FVector2D(20.0f, 20.0f);
        CardBrush.OutlineSettings.Color = FLinearColor(0.4f, 0.35f, 0.5f, 0.9f);
        CardBrush.OutlineSettings.Width = 2.0f;
        CardBorder->SetBrush(CardBrush);
    }
}
```

### 3.2 标题区域

```cpp
// WorldSetupWidget.cpp

void UWorldSetupWidget::ApplyTitleStyle()
{
    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("世界创建")));
        TitleText->SetColorAndOpacity(FLinearColor(1.0f, 0.9f, 0.7f, 1.0f));
        FSlateFontInfo TitleFont = TitleText->GetFont();
        TitleFont.Size = 36;
        TitleFont.OutlineSettings.OutlineSize = 2;
        TitleFont.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.8f);
        TitleText->SetFont(TitleFont);

        TitleText->SetShadowOffset(FVector2D(2.0f, 2.0f));
        TitleText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
    }
}
```

### 3.3 游戏风格选择器

**目标**：将 `UComboBoxString` 替换为选项卡式按钮布局，带图标和选中高亮。

**新增属性**：

```cpp
// WorldSetupWidget.h - 新增属性

class UHorizontalBox;
class UButton;
class UTextBlock;
class UImage;

/** Style button container */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UHorizontalBox> StyleButtonContainer;

/** Array of style selection buttons */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UButton> StyleButton_0;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UButton> StyleButton_1;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UButton> StyleButton_2;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UButton> StyleButton_3;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UButton> StyleButton_4;

/** Style button labels */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> StyleLabel_0;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> StyleLabel_1;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> StyleLabel_2;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> StyleLabel_3;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> StyleLabel_4;

/** Style button icon images */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> StyleIcon_0;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> StyleIcon_1;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> StyleIcon_2;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> StyleIcon_3;

UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> StyleIcon_4;

// Style names mapping
static const TArray<FString> GameStyleNames;

// New callback
UFUNCTION()
void OnStyleButtonClicked(int32 StyleIndex);

// Style button state
UPROPERTY()
int32 SelectedStyleIndex = 0;

// Helper
void ApplyStyleButtonStyles();
void UpdateStyleButtonSelection(int32 NewIndex);
void SetupStyleButtons();
```

**风格按钮设置和选中逻辑**：

```cpp
// WorldSetupWidget.cpp

const TArray<FString> UWorldSetupWidget::GameStyleNames = {
    TEXT("Medieval Fantasy"),
    TEXT("Sci-Fi Future"),
    TEXT("Modern Horror"),
    TEXT("Post-Apocalyptic"),
    TEXT("Custom")
};

void UWorldSetupWidget::SetupStyleButtons()
{
    // Create an array of button pointers for easy iteration
    TArray<TObjectPtr<UButton>> Buttons = {
        StyleButton_0, StyleButton_1, StyleButton_2, StyleButton_3, StyleButton_4
    };
    TArray<TObjectPtr<UTextBlock>> Labels = {
        StyleLabel_0, StyleLabel_1, StyleLabel_2, StyleLabel_3, StyleLabel_4
    };

    for (int32 i = 0; i < Buttons.Num() && i < GameStyleNames.Num(); ++i)
    {
        if (Buttons[i])
        {
            int32 Index = i;
            Buttons[i]->OnClicked.AddDynamic(this, &UWorldSetupWidget::OnStyleButtonClicked);

            // Set padding
            Buttons[i]->SetPadding(FMargin(12.0f, 8.0f));
        }

        if (Labels[i])
        {
            Labels[i]->SetText(FText::FromString(GameStyleNames[i]));
            Labels[i]->SetColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f, 1.0f));
            FSlateFontInfo LabelFont = Labels[i]->GetFont();
            LabelFont.Size = 13;
            Labels[i]->SetFont(LabelFont);
        }
    }

    // Set initial selection
    UpdateStyleButtonSelection(0);
}

void UWorldSetupWidget::ApplyStyleButtonStyles()
{
    TArray<TObjectPtr<UButton>> Buttons = {
        StyleButton_0, StyleButton_1, StyleButton_2, StyleButton_3, StyleButton_4
    };

    for (int32 i = 0; i < Buttons.Num(); ++i)
    {
        if (!Buttons[i])
        {
            continue;
        }

        // Normal state brush
        FSlateBrush NormalBrush;
        NormalBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        NormalBrush.TintColor = FLinearColor(0.15f, 0.15f, 0.2f, 0.9f);
        NormalBrush.OutlineSettings.CornerRadii = FVector2D(10.0f, 10.0f);
        NormalBrush.OutlineSettings.Color = FLinearColor(0.3f, 0.3f, 0.4f, 0.8f);
        NormalBrush.OutlineSettings.Width = 1.5f;
        Buttons[i]->SetBackgroundBrush(NormalBrush);

        // Hover state brush
        FSlateBrush HoverBrush;
        HoverBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        HoverBrush.TintColor = FLinearColor(0.25f, 0.22f, 0.3f, 0.95f);
        HoverBrush.OutlineSettings.CornerRadii = FVector2D(10.0f, 10.0f);
        HoverBrush.OutlineSettings.Color = FLinearColor(0.5f, 0.5f, 0.6f, 1.0f);
        HoverBrush.OutlineSettings.Width = 2.0f;
        Buttons[i]->SetHoverBrush(HoverBrush);

        // Pressed state brush
        FSlateBrush PressedBrush;
        PressedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        PressedBrush.TintColor = FLinearColor(0.3f, 0.28f, 0.35f, 1.0f);
        PressedBrush.OutlineSettings.CornerRadii = FVector2D(10.0f, 10.0f);
        PressedBrush.OutlineSettings.Color = FLinearColor(0.6f, 0.55f, 0.7f, 1.0f);
        PressedBrush.OutlineSettings.Width = 2.0f;
        Buttons[i]->SetPressedBrush(PressedBrush);
    }

    // Apply selected style to initial button
    UpdateStyleButtonSelection(SelectedStyleIndex);
}

void UWorldSetupWidget::UpdateStyleButtonSelection(int32 NewIndex)
{
    SelectedStyleIndex = NewIndex;

    TArray<TObjectPtr<UButton>> Buttons = {
        StyleButton_0, StyleButton_1, StyleButton_2, StyleButton_3, StyleButton_4
    };
    TArray<TObjectPtr<UTextBlock>> Labels = {
        StyleLabel_0, StyleLabel_1, StyleLabel_2, StyleLabel_3, StyleLabel_4
    };

    for (int32 i = 0; i < Buttons.Num(); ++i)
    {
        if (i == NewIndex)
        {
            // Selected state
            FSlateBrush SelectedBrush;
            SelectedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
            SelectedBrush.TintColor = FLinearColor(0.3f, 0.25f, 0.15f, 1.0f);
            SelectedBrush.OutlineSettings.CornerRadii = FVector2D(10.0f, 10.0f);
            SelectedBrush.OutlineSettings.Color = FLinearColor(1.0f, 0.85f, 0.5f, 1.0f);
            SelectedBrush.OutlineSettings.Width = 2.5f;
            Buttons[i]->SetBackgroundBrush(SelectedBrush);

            if (Labels[i])
            {
                Labels[i]->SetColorAndOpacity(FLinearColor(1.0f, 0.9f, 0.7f, 1.0f));
            }
        }
        else
        {
            // Reset to normal state
            FSlateBrush NormalBrush;
            NormalBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
            NormalBrush.TintColor = FLinearColor(0.15f, 0.15f, 0.2f, 0.9f);
            NormalBrush.OutlineSettings.CornerRadii = FVector2D(10.0f, 10.0f);
            NormalBrush.OutlineSettings.Color = FLinearColor(0.3f, 0.3f, 0.4f, 0.8f);
            NormalBrush.OutlineSettings.Width = 1.5f;
            Buttons[i]->SetBackgroundBrush(NormalBrush);

            if (Labels[i])
            {
                Labels[i]->SetColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 0.9f));
            }
        }
    }
}

void UWorldSetupWidget::OnStyleButtonClicked(int32 StyleIndex)
{
    if (StyleIndex >= 0 && StyleIndex < GameStyleNames.Num())
    {
        UpdateStyleButtonSelection(StyleIndex);

        // Also update the original combo box if present
        if (GameStyleSelector)
        {
            GameStyleSelector->SetSelectedOption(GameStyleNames[StyleIndex]);
        }
    }
}
```

### 3.4 输入框统一样式

```cpp
// WorldSetupWidget.cpp

void UWorldSetupWidget::ApplyInputFieldStyles()
{
    TArray<UEditableTextBox*> TextBoxes = { WorldPromptTextBox, PlayerCharacterNameBox };

    for (UEditableTextBox* TextBox : TextBoxes)
    {
        if (!TextBox)
        {
            continue;
        }

        // Background brush
        FSlateBrush BackgroundBrush;
        BackgroundBrush.TintColor = FLinearColor(0.08f, 0.08f, 0.12f, 1.0f);
        TextBox->SetBackgroundBrush(BackgroundBrush);

        // Readonly background (for reference)
        FSlateBrush ReadonlyBrush;
        ReadonlyBrush.TintColor = FLinearColor(0.06f, 0.06f, 0.09f, 0.8f);
        TextBox->SetReadonlyBrush(ReadonlyBrush);

        // Font
        FSlateFontInfo InputFont = TextBox->GetFont();
        InputFont.Size = 16;
        TextBox->SetFont(InputFont);
        TextBox->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.95f, 1.0f));

        // Padding
        TextBox->SetPadding(FMargin(14.0f, 10.0f));
    }
}
```

### 3.5 开始按钮美化

```cpp
// WorldSetupWidget.cpp

void UWorldSetupWidget::ApplyStartButtonStyle()
{
    if (!StartGameButton)
    {
        return;
    }

    // Normal state - gradient effect via tint
    FSlateBrush NormalBrush;
    NormalBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    NormalBrush.TintColor = FLinearColor(0.35f, 0.25f, 0.1f, 1.0f);
    NormalBrush.OutlineSettings.CornerRadii = FVector2D(12.0f, 12.0f);
    NormalBrush.OutlineSettings.Color = FLinearColor(0.8f, 0.65f, 0.3f, 1.0f);
    NormalBrush.OutlineSettings.Width = 2.0f;
    StartGameButton->SetBackgroundBrush(NormalBrush);

    // Hover state - brighter with glow border
    FSlateBrush HoverBrush;
    HoverBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    HoverBrush.TintColor = FLinearColor(0.5f, 0.38f, 0.15f, 1.0f);
    HoverBrush.OutlineSettings.CornerRadii = FVector2D(12.0f, 12.0f);
    HoverBrush.OutlineSettings.Color = FLinearColor(1.0f, 0.9f, 0.5f, 1.0f);
    HoverBrush.OutlineSettings.Width = 3.0f;
    StartGameButton->SetHoverBrush(HoverBrush);

    // Pressed state
    FSlateBrush PressedBrush;
    PressedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    PressedBrush.TintColor = FLinearColor(0.6f, 0.45f, 0.2f, 1.0f);
    PressedBrush.OutlineSettings.CornerRadii = FVector2D(12.0f, 12.0f);
    PressedBrush.OutlineSettings.Color = FLinearColor(1.0f, 0.95f, 0.6f, 1.0f);
    PressedBrush.OutlineSettings.Width = 3.0f;
    StartGameButton->SetPressedBrush(PressedBrush);

    // Generous padding for the large button
    StartGameButton->SetPadding(FMargin(32.0f, 16.0f));
}
```

> **蓝图中的操作**：在 Widget Blueprint 中为 `StartGameButton` 添加 `UTextBlock` 子控件，设置文字为"开始冒险"，字体大小 22，颜色为暖白色。可在蓝图的动画编辑器中为按钮添加 Hover 放大动画（通过 `BindAnimation` 绑定到 `OnHovered`/`OnUnhovered` 事件）。

---

## 4. LoadingWidget 美化修改

当前 `ULoadingWidget` 的主要问题：
- 仅有文本和跳动点，缺乏视觉丰富度
- 无进度环或进度条
- 无背景氛围效果
- 文字缺乏发光效果

### 4.1 进度指示

**目标**：添加 `UCircularThrobber` 进度环和 `UProgressBar`。

**新增 BindWidget**：

```cpp
// LoadingWidget.h - 新增属性

class UCircularThrobber;
class UProgressBar;
class UImage;
class UBorder;
class UOverlay;

/** Animated circular progress indicator */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UCircularThrobber> ProgressThrobber;

/** Linear progress bar */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UProgressBar> ProgressBar;

/** Background overlay image */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> BackgroundImage;

/** Content container border */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UBorder> ContentBorder;

/** Root overlay */
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UOverlay> RootOverlay;

/** Set progress value (0.0 - 1.0) */
UFUNCTION(BlueprintCallable, Category = "Loading")
void SetProgress(float InProgress);
```

**NativeConstruct 中设置进度样式**：

```cpp
// LoadingWidget.cpp
#include "Components/CircularThrobber.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Styling/SlateBrush.h"

void ULoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Setup background atmosphere
    ApplyBackgroundStyle();

    // Setup text with glow effect
    ApplyTextGlowStyle();

    // Setup progress indicators
    ApplyProgressStyle();

    UpdateDotsText();
}

void ULoadingWidget::ApplyBackgroundStyle()
{
    // Dark gradient-like background
    if (BackgroundImage)
    {
        BackgroundImage->SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.1f, 0.95f));
    }

    // Content card border
    if (ContentBorder)
    {
        FSlateBrush CardBrush;
        CardBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        CardBrush.TintColor = FLinearColor(0.08f, 0.08f, 0.15f, 0.9f);
        CardBrush.OutlineSettings.CornerRadii = FVector2D(24.0f, 24.0f);
        CardBrush.OutlineSettings.Color = FLinearColor(0.25f, 0.2f, 0.4f, 0.8f);
        CardBrush.OutlineSettings.Width = 2.0f;
        ContentBorder->SetBrush(CardBrush);
    }
}

void ULoadingWidget::ApplyTextGlowStyle()
{
    // Main loading text with glow
    if (LoadingText)
    {
        LoadingText->SetText(FText::FromString(BaseLoadingText));

        FSlateFontInfo FontInfo = LoadingText->GetFont();
        FontInfo.Size = 34;
        FontInfo.OutlineSettings.OutlineSize = 3;
        FontInfo.OutlineSettings.OutlineColor = FLinearColor(0.2f, 0.4f, 0.8f, 0.8f);
        LoadingText->SetFont(FontInfo);

        LoadingText->SetColorAndOpacity(FLinearColor(0.95f, 0.95f, 1.0f, 1.0f));
        LoadingText->SetAutoWrapText(true);

        // Shadow for glow effect (use a bright shadow color)
        LoadingText->SetShadowOffset(FVector2D(0.0f, 0.0f));
        LoadingText->SetShadowColorAndOpacity(FLinearColor(0.3f, 0.5f, 1.0f, 0.6f));
    }

    // Animated dots with glow
    if (DotsText)
    {
        FSlateFontInfo FontInfo = DotsText->GetFont();
        FontInfo.Size = 52;
        FontInfo.OutlineSettings.OutlineSize = 2;
        FontInfo.OutlineSettings.OutlineColor = FLinearColor(0.2f, 0.4f, 0.8f, 0.6f);
        DotsText->SetFont(FontInfo);

        DotsText->SetColorAndOpacity(FLinearColor(0.3f, 0.7f, 1.0f, 1.0f));

        // Glow shadow
        DotsText->SetShadowOffset(FVector2D(0.0f, 0.0f));
        DotsText->SetShadowColorAndOpacity(FLinearColor(0.2f, 0.5f, 1.0f, 0.5f));
    }
}

void ULoadingWidget::ApplyProgressStyle()
{
    // Circular throbber
    if (ProgressThrobber)
    {
        ProgressThrobber->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ProgressThrobber->SetColorAndOpacity(FLinearColor(0.3f, 0.6f, 1.0f, 1.0f));
    }

    // Progress bar
    if (ProgressBar)
    {
        ProgressBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ProgressBar->SetPercent(0.0f);
        ProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);

        // Style the background
        FSlateBrush BgBrush;
        BgBrush.TintColor = FLinearColor(0.15f, 0.15f, 0.25f, 0.8f);
        ProgressBar->SetBackgroundBrush(BgBrush);

        // Style the fill bar
        FSlateBrush FillBrush;
        FillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        FillBrush.TintColor = FLinearColor(0.3f, 0.6f, 1.0f, 1.0f);
        FillBrush.OutlineSettings.CornerRadii = FVector2D(4.0f, 4.0f);
        ProgressBar->SetFillBrush(FillBrush);

        // Style the marquee (loading animation)
        FSlateBrush MarqueeBrush;
        MarqueeBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        MarqueeBrush.TintColor = FLinearColor(0.5f, 0.7f, 1.0f, 1.0f);
        ProgressBar->SetMarqueeBrush(MarqueeBrush);
    }
}

void ULoadingWidget::SetProgress(float InProgress)
{
    float ClampedProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);

    if (ProgressBar)
    {
        ProgressBar->SetPercent(ClampedProgress);
    }
}
```

### 4.2 背景氛围

**目标**：添加粒子效果和暗色渐变背景。

粒子效果建议在 UMG 蓝图中操作：

1. 在 Widget Blueprint 中添加 `UNiagaraWidgetComponent`（需要 Niagara UI Widget 插件启用）
2. 设置 Niagara System 为轻量级的漂浮光点/烟雾效果
3. 将其放置在背景层，设置 ZOrder 低于其他控件

如果不想引入 Niagara 依赖，也可以使用 `UMaterialInstance` 的 UV 动画来模拟简单的粒子/光效：

```cpp
// LoadingWidget.cpp - 在 NativeConstruct 中

#include "Materials/MaterialInstanceDynamic.h"
#include "Components/Image.h"

void ULoadingWidget::SetupAnimatedBackground()
{
    if (BackgroundImage)
    {
        // If the background image has a material instance assigned
        UMaterialInterface* MatInterface = BackgroundImage->GetBrush().GetResourceObject()
            ? Cast<UMaterialInterface>(BackgroundImage->GetBrush().GetResourceObject()) : nullptr;

        if (MatInterface)
        {
            UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(MatInterface, this);
            BackgroundImage->SetBrushFromMaterial(DynMat);

            // Set animation parameters (assuming material has these parameters)
            if (DynMat)
            {
                DynMat->SetScalarParameterValue(TEXT("AnimationSpeed"), 0.3f);
                DynMat->SetScalarParameterValue(TEXT("Opacity"), 0.15f);
                DynMat->SetVectorParameterValue(
                    TEXT("GlowColor"),
                    FLinearColor(0.2f, 0.3f, 0.6f, 1.0f));
            }
        }
    }
}
```

### 4.3 跳动点动画

**目标**：将文本跳动点改为使用 UMG Animation Blueprint 实现更流畅的跳动效果。

> **推荐方案**：在 Widget Blueprint 的 Animations 面板中创建一个名为 `DotBounce` 的动画：
> 1. 为 `DotsText` 添加缩放关键帧（Scale Y: 1.0 -> 0.3 -> 1.0，时长 0.5 秒）
> 2. 为 `DotsText` 添加位移动画（Offset Y: 0 -> -5 -> 0，时长 0.5 秒）
> 3. 设置为循环动画
> 4. 在 NativeConstruct 中使用 `BindAnimation` 播放：

```cpp
// LoadingWidget.h - 新增
UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
TObjectPtr<UWidgetAnimation> DotBounceAnimation;

// LoadingWidget.cpp - 在 NativeConstruct 末尾
if (DotBounceAnimation)
{
    PlayAnimation(DotBounceAnimation, 0.0f, 0);
}
```

---

## 5. 通用样式应用方法

### 5.1 UMG 蓝图中使用控件模板

在 UMG Editor 中创建控件模板的步骤：

1. **创建模板 Widget Blueprint**：
   - 右键 Content Browser -> User Interface -> Widget Blueprint
   - 命名为 `WBP_StyledButton`、`WBP_StyledPanel` 等
   - 在模板中完成所有样式设置（Brush、Font、Padding 等）

2. **在其他 Widget 中引用模板**：
   - 在父 Widget 的 Designer 面板中，从 Palette 拖入 `Child Widget`
   - 选择模板类（如 `WBP_StyledButton`）
   - 通过 Variables 面板设置暴露的参数

3. **使用 Widget Switcher 切换不同风格版本**：
   - 创建多个同名模板，不同风格（如 `WBP_StyledButton_Fantasy`、`WBP_StyledButton_SciFi`）
   - 在运行时通过代码设置 ActiveChildIndex 切换

### 5.2 常用样式 API 汇总

```cpp
// === Color & Opacity ===
TextBlock->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.5f, 1.0f));
Image->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.8f));

// === Font ===
FSlateFontInfo FontInfo;
FontInfo.FontObject = LoadObject<UObject>(nullptr, TEXT("/Game/UI/Fonts/MyFont"));
FontInfo.Size = 24;
FontInfo.OutlineSettings.OutlineSize = 2;
FontInfo.OutlineSettings.OutlineColor = FLinearColor::Black;
TextBlock->SetFont(FontInfo);

// === Shadow ===
TextBlock->SetShadowOffset(FVector2D(1.0f, 1.0f));
TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));

// === Brush ===
FSlateBrush Brush;
Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
Brush.TintColor = FLinearColor(0.2f, 0.2f, 0.3f, 1.0f);
Brush.OutlineSettings.CornerRadii = FVector2D(10.0f, 10.0f);
Brush.OutlineSettings.Color = FLinearColor(0.5f, 0.5f, 0.6f, 1.0f);
Brush.OutlineSettings.Width = 2.0f;
Border->SetBrush(Brush);
Button->SetBackgroundBrush(Brush);

// === Texture Brush ===
UTexture2D* MyTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/MyTexture"));
FSlateBrush TexBrush;
TexBrush.SetResourceObject(MyTex);
TexBrush.ImageSize = FVector2D(64.0f, 64.0f);
Image->SetBrush(TexBrush);
// Or use the convenience function:
Image->SetBrushFromTexture(MyTex);
Image->SetBrushFromMaterial(MaterialInstance);

// === Padding ===
Button->SetPadding(FMargin(16.0f, 10.0f));
EditableTextBox->SetPadding(FMargin(12.0f, 8.0f));
VerticalBoxSlot->SetPadding(FMargin(4.0f, 8.0f));

// === Visibility ===
Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);  // Visible but not clickable
Widget->SetVisibility(ESlateVisibility::Collapsed);              // Hidden, takes no space
Widget->SetVisibility(ESlateVisibility::Hidden);                // Hidden, takes space

// === Material Instance Dynamic ===
UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
DynMat->SetScalarParameterValue(TEXT("ParamName"), 0.5f);
DynMat->SetVectorParameterValue(TEXT("ColorParam"), FLinearColor::Red);
Image->SetBrushFromMaterial(DynMat);
```

### 5.3 动画使用 UMG Animation Blueprint

推荐使用 UMG 内置的 Animation 系统，而非在 Tick 中硬编码动画逻辑：

**C++ 中播放/停止动画**：

```cpp
// In header
UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
TObjectPtr<UWidgetAnimation> MyAnimation;

// Play animation once
PlayAnimation(MyAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f);

// Play animation in loop
PlayAnimation(MyAnimation, 0.0f, 0);

// Play animation reversed
PlayAnimation(MyAnimation, 0.0f, 1, EUMGSequencePlayMode::Reverse, 1.0f);

// Stop animation
StopAnimation(MyAnimation);
```

**在蓝图中绑定动画到事件**：

1. 在 Animations 面板创建动画（如 `FadeIn`、`ButtonHover`）
2. 在 Graph 面板中，从事件节点（如 `OnHovered`）拖出并搜索 `Play Animation`
3. 选择要播放的动画，设置播放参数

### 5.4 材质实例动态切换

```cpp
// LoadingWidget.cpp - 动态切换材质参数示例
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/Image.h"

void ULoadingWidget::UpdateGlowEffect(float Intensity, const FLinearColor& GlowColor)
{
    if (!BackgroundImage)
    {
        return;
    }

    // Get or create dynamic material instance
    UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(
        BackgroundImage->GetDynamicMaterial());

    if (!DynMat)
    {
        // Fallback: create from brush resource
        UObject* BrushResource = BackgroundImage->GetBrush().GetResourceObject();
        UMaterialInterface* BaseMat = Cast<UMaterialInterface>(BrushResource);
        if (BaseMat)
        {
            DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
            BackgroundImage->SetBrushFromMaterial(DynMat);
        }
    }

    if (DynMat)
    {
        DynMat->SetScalarParameterValue(TEXT("GlowIntensity"), Intensity);
        DynMat->SetVectorParameterValue(TEXT("GlowColor"), GlowColor);
    }
}
```

---

## 附录：完整文件清单

| 文件 | 说明 |
|------|------|
| `DialogueWidget.h` | NPC 对话 Widget 头文件（需新增属性和函数声明） |
| `DialogueWidget.cpp` | NPC 对话 Widget 实现（需新增样式设置函数） |
| `DialogueMessageEntry.h` | 对话消息条目子控件头文件（新增文件） |
| `DialogueMessageEntry.cpp` | 对话消息条目子控件实现（新增文件） |
| `WorldSetupWidget.h` | 世界创建 Widget 头文件（需新增属性和函数声明） |
| `WorldSetupWidget.cpp` | 世界创建 Widget 实现（需新增样式设置函数） |
| `LoadingWidget.h` | 加载 Widget 头文件（需新增属性和函数声明） |
| `Loading