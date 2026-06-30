# UI 美术向优化代码修改文档

## 1. 概述

本文档基于现有三个 Widget 的 C++ 代码，进行美术视觉升级，包括背景样式、文字排版、颜色方案、边框效果、动画过渡等。

### 现有代码位置

- `Source/PluginExample/Public/DialogueWidget.h` / `Private/DialogueWidget.cpp`
- `Source/PluginExample/Public/WorldSetupWidget.h` / `Private/WorldSetupWidget.cpp`
- `Source/PluginExample/Public/LoadingWidget.h` / `Private/LoadingWidget.cpp`

### 现有 Widget 元素清单

**DialogueWidget:** NPCNameText(UTextBlock), HistoryScrollBox(UScrollBox), HistoryContainer(UVerticalBox), DialogueText(UTextBlock), ChoicesBox(UVerticalBox), ContinueButton(UButton), InputTextBox(UEditableTextBox), SendButton(UButton), InputContainer(USizeBox), ChoicesContainer(USizeBox), ThinkingText(UTextBlock)

**WorldSetupWidget:** WorldPromptTextBox(UEditableTextBox), StartGameButton(UButton), GameStyleSelector(UComboBoxString), PlayerCharacterNameBox(UEditableTextBox)

**LoadingWidget:** LoadingText(UTextBlock), DotsText(UTextBlock)

---

## 2. DialogueWidget 美术优化

### 2.1 推荐新增 BindWidget 成员

在 `DialogueWidget.h` 的 public 段中添加以下属性：

```cpp
class UBorder;

// ... 在 DialogueWidget 类内 ...

public:
    /** Root background border panel */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UBorder> BackgroundBorder;

    /** NPC name plate border */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> NPCNamePlateBorder;

    /** Dialogue text area border (parchment/card background) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> DialogueAreaBorder;

    /** Input area border */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> InputAreaBorder;
```

> **说明**：`BindWidget` 要求蓝图中必须存在同名控件，`BindWidgetOptional` 表示可选绑定。新增的 `UBorder` 控件需要在 UMG 蓝图编辑器中手动创建并命名为对应名称。

### 2.2 定义 UI 风格配置结构体

建议将所有可配置的颜色和字体参数统一到一个 `USTRUCT` 中，避免硬编码散布在各处。在 `DialogueWidget.h` 文件顶部（类声明之前）定义：

```cpp
USTRUCT(BlueprintType)
struct FUIStyleConfig
{
    GENERATED_BODY()

    // 背景色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor BackgroundColor = FLinearColor(0.05f, 0.05f, 0.1f, 0.85f);

    // NPC名称颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor NPCNameColor = FLinearColor(1.0f, 0.85f, 0.5f, 1.0f);

    // 对话文本颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor DialogueTextColor = FLinearColor(0.95f, 0.95f, 1.0f, 1.0f);

    // 玩家消息颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor PlayerMessageColor = FLinearColor(0.3f, 0.8f, 1.0f, 1.0f);

    // NPC消息颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor NPCMessageColor = FLinearColor(1.0f, 0.8f, 0.3f, 1.0f);

    // 输入框背景色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor InputBgColor = FLinearColor(0.1f, 0.1f, 0.15f, 0.9f);

    // 思考指示器颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor ThinkingColor = FLinearColor(0.5f, 0.7f, 1.0f, 1.0f);

    // 主色调（用于按钮高亮等）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor AccentColor = FLinearColor(0.3f, 0.6f, 1.0f, 1.0f);

    // 字体大小配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 NPCNameFontSize = 28;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 DialogueTextFontSize = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 MessageSpeakerFontSize = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 MessageTextFontSize = 14;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 ThinkingFontSize = 16;

    // 边框圆角大小
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    float BorderRadius = 8.0f;
};
```

然后在 `UDialogueWidget` 类中添加风格配置实例：

```cpp
// DialogueWidget.h - 在类内添加

    /** UI style configuration - editable in Blueprint details panel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Style")
    FUIStyleConfig StyleConfig;
```

### 2.3 NativeConstruct 中的样式增强代码

以下代码替换 `DialogueWidget.cpp` 中 `NativeConstruct` 的样式设置部分。原有按钮绑定逻辑保持不变，仅替换硬编码样式部分。

```cpp
// DialogueWidget.cpp
#include "Components/Border.h"
#include "Styling/SlateBrush.h"

void UDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // === 原有按钮绑定代码（保持不变） ===

    if (SendButton)
    {
        SendButton->OnClicked.AddDynamic(this, &UDialogueWidget::OnSendClicked);
    }

    if (ContinueButton)
    {
        ContinueButton->OnClicked.AddDynamic(this, &UDialogueWidget::OnContinueClicked);
    }

    if (InputTextBox)
    {
        InputTextBox->OnTextCommitted.AddDynamic(this, &UDialogueWidget::OnInputCommitted);
    }

    // === 新增：背景边框样式 ===
    if (BackgroundBorder)
    {
        // 设置半透明背景（推荐在蓝图端使用 Brush 材质实现磨砂玻璃效果）
        FSlateBrush BgBrush;
        BgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        BgBrush.TintColor = StyleConfig.BackgroundColor;
        BgBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius, StyleConfig.BorderRadius);
        BgBrush.OutlineSettings.Color = FLinearColor(StyleConfig.AccentColor.R * 0.5f,
                                                       StyleConfig.AccentColor.G * 0.5f,
                                                       StyleConfig.AccentColor.B * 0.5f, 0.6f);
        BgBrush.OutlineSettings.Width = 1.5f;
        BackgroundBorder->SetBrush(BgBrush);
    }

    // === 新增：NPC 名称板边框 ===
    if (NPCNamePlateBorder)
    {
        FSlateBrush PlateBrush;
        PlateBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        PlateBrush.TintColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.3f);
        PlateBrush.OutlineSettings.CornerRadii = FVector2D(6.0f, 6.0f);
        PlateBrush.OutlineSettings.Color = FLinearColor(
            StyleConfig.NPCNameColor.R * 0.6f,
            StyleConfig.NPCNameColor.G * 0.6f,
            StyleConfig.NPCNameColor.B * 0.6f, 0.5f);
        PlateBrush.OutlineSettings.Width = 1.0f;
        NPCNamePlateBorder->SetBrush(PlateBrush);
    }

    // === 新增：对话区域边框（羊皮纸/卡片背景） ===
    if (DialogueAreaBorder)
    {
        FSlateBrush AreaBrush;
        AreaBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        AreaBrush.TintColor = FLinearColor(0.08f, 0.08f, 0.12f, 0.7f);
        AreaBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius, StyleConfig.BorderRadius);
        AreaBrush.OutlineSettings.Color = FLinearColor(0.3f, 0.3f, 0.4f, 0.5f);
        AreaBrush.OutlineSettings.Width = 1.0f;
        DialogueAreaBorder->SetBrush(AreaBrush);
    }

    // === 新增：输入区域边框 ===
    if (InputAreaBorder)
    {
        FSlateBrush InputBorderBrush;
        InputBorderBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        InputBorderBrush.TintColor = StyleConfig.InputBgColor;
        InputBorderBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius, StyleConfig.BorderRadius);
        InputBorderBrush.OutlineSettings.Color = FLinearColor(
            StyleConfig.AccentColor.R * 0.4f,
            StyleConfig.AccentColor.G * 0.4f,
            StyleConfig.AccentColor.B * 0.4f, 0.6f);
        InputBorderBrush.OutlineSettings.Width = 1.0f;
        InputAreaBorder->SetBrush(InputBorderBrush);
    }

    // === NPC 名称样式（使用 StyleConfig 替换硬编码颜色） ===
    if (NPCNameText)
    {
        FSlateFontInfo NameFont = NPCNameText->GetFont();
        NameFont.Size = StyleConfig.NPCNameFontSize;  // 从 24 增大到 28
        NPCNameText->SetFont(NameFont);

        // 设置可配置的颜色（原硬编码金色 FLinearColor(1.0f, 0.85f, 0.5f, 1.0f)）
        NPCNameText->SetColorAndOpacity(StyleConfig.NPCNameColor);

        // 添加柔和发光模拟效果：通过 Shadow + Offset=0 实现辉光叠加
        NPCNameText->SetShadowOffset(FVector2D(0.0f, 0.0f));
        NPCNameText->SetShadowColorAndOpacity(FLinearColor(
            StyleConfig.NPCNameColor.R,
            StyleConfig.NPCNameColor.G,
            StyleConfig.NPCNameColor.B, 0.4f));
    }

    // === 对话文本样式（使用 StyleConfig 替换硬编码颜色） ===
    if (DialogueText)
    {
        FSlateFontInfo DialogueFont = DialogueText->GetFont();
        DialogueFont.Size = StyleConfig.DialogueTextFontSize;  // 从 18 增大到 20
        DialogueText->SetFont(DialogueFont);

        // 使用可配置颜色替换原硬编码 FLinearColor(0.95f, 0.95f, 1.0f, 1.0f)
        DialogueText->SetColorAndOpacity(StyleConfig.DialogueTextColor);

        DialogueText->SetAutoWrapText(true);

        // 增加行高（通过 Shadow Offset 模拟行间距）
        DialogueText->SetShadowOffset(FVector2D(0.5f, 1.0f));
        DialogueText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.15f));
    }

    // === 思考指示器样式 ===
    if (ThinkingText)
    {
        FSlateFontInfo ThinkingFont = ThinkingText->GetFont();
        ThinkingFont.Size = StyleConfig.ThinkingFontSize;
        ThinkingText->SetFont(ThinkingFont);

        // 使用可配置颜色替换原硬编码 FLinearColor(0.5f, 0.7f, 1.0f, 1.0f)
        ThinkingText->SetColorAndOpacity(StyleConfig.ThinkingColor);

        // 添加柔和辉光
        ThinkingText->SetShadowOffset(FVector2D(0.0f, 0.0f));
        ThinkingText->SetShadowColorAndOpacity(FLinearColor(
            StyleConfig.ThinkingColor.R,
            StyleConfig.ThinkingColor.G,
            StyleConfig.ThinkingColor.B, 0.3f));
    }

    // Hide input initially
    HideInputField();
}
```

### 2.4 AddMessageToHistory 的视觉增强

以下代码替换原有的 `AddMessageToHistory` 方法，为每条消息添加背景气泡效果：

```cpp
// DialogueWidget.cpp - 头文件中需新增前置声明
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"

void UDialogueWidget::AddMessageToHistory(const FString& SpeakerName, const FString& Message, bool bIsPlayer)
{
    if (!HistoryContainer)
    {
        return;
    }

    // === 创建消息气泡结构 ===
    // 外层容器：UBorder 作为消息气泡背景
    UBorder* BubbleBorder = NewObject<UBorder>(HistoryContainer);

    FSlateBrush BubbleBrush;
    BubbleBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    if (bIsPlayer)
    {
        // 玩家气泡：蓝色调
        BubbleBrush.TintColor = FLinearColor(0.15f, 0.25f, 0.45f, 0.85f);
        BubbleBrush.OutlineSettings.Color = FLinearColor(
            StyleConfig.PlayerMessageColor.R * 0.7f,
            StyleConfig.PlayerMessageColor.G * 0.7f,
            StyleConfig.PlayerMessageColor.B * 0.7f, 0.8f);
    }
    else
    {
        // NPC气泡：暖色调
        BubbleBrush.TintColor = FLinearColor(0.25f, 0.18f, 0.1f, 0.85f);
        BubbleBrush.OutlineSettings.Color = FLinearColor(
            StyleConfig.NPCMessageColor.R * 0.7f,
            StyleConfig.NPCMessageColor.G * 0.7f,
            StyleConfig.NPCMessageColor.B * 0.7f, 0.8f);
    }
    BubbleBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius, StyleConfig.BorderRadius);
    BubbleBrush.OutlineSettings.Width = 1.0f;
    BubbleBorder->SetBrush(BubbleBrush);

    // 内层容器：UVerticalBox 存放说话者和消息
    UVerticalBox* BubbleContent = NewObject<UVerticalBox>(BubbleBorder);
    BubbleBorder->AddChild(BubbleContent);

    // === 创建说话者名称 ===
    UTextBlock* SpeakerText = NewObject<UTextBlock>(BubbleContent);
    if (bIsPlayer)
    {
        SpeakerText->SetText(FText::FromString(TEXT("You")));
    }
    else
    {
        SpeakerText->SetText(FText::FromString(SpeakerName));
    }

    // 说话者名称颜色和粗体（使用 StyleConfig）
    SpeakerText->SetColorAndOpacity(bIsPlayer ? StyleConfig.PlayerMessageColor : StyleConfig.NPCMessageColor);
    FSlateFontInfo SpeakerFont = SpeakerText->GetFont();
    SpeakerFont.Size = StyleConfig.MessageSpeakerFontSize;
    SpeakerFont.TypefaceFontName = FName(TEXT("Bold"));
    SpeakerText->SetFont(SpeakerFont);
    SpeakerText->SetAutoWrapText(true);

    // 说话者名称 Slot 设置上内边距
    UVerticalBoxSlot* SpeakerSlot = Cast<UVerticalBoxSlot>(BubbleContent->AddChild(SpeakerText));
    if (SpeakerSlot)
    {
        SpeakerSlot->SetPadding(FMargin(8.0f, 6.0f, 8.0f, 2.0f));
        SpeakerSlot->SetHorizontalAlignment(HAlign_Left);
    }

    // === 创建消息内容 ===
    UTextBlock* MessageText = NewObject<UTextBlock>(BubbleContent);
    MessageText->SetText(FText::FromString(Message));
    MessageText->SetColorAndOpacity(FLinearColor(0.95f, 0.95f, 1.0f, 1.0f));
    FSlateFontInfo MessageFont = MessageText->GetFont();
    MessageFont.Size = StyleConfig.MessageTextFontSize;
    MessageText->SetFont(MessageFont);
    MessageText->SetAutoWrapText(true);

    // 消息 Slot 设置下内边距
    UVerticalBoxSlot* MessageSlot = Cast<UVerticalBoxSlot>(BubbleContent->AddChild(MessageText));
    if (MessageSlot)
    {
        MessageSlot->SetPadding(FMargin(8.0f, 2.0f, 8.0f, 6.0f));
        MessageSlot->SetHorizontalAlignment(HAlign_Left);
    }

    // === 对齐方式：玩家消息右对齐，NPC消息左对齐 ===
    UVerticalBoxSlot* BubbleSlot = Cast<UVerticalBoxSlot>(HistoryContainer->AddChild(BubbleBorder));
    if (BubbleSlot)
    {
        // 消息间距
        BubbleSlot->SetPadding(FMargin(4.0f, 4.0f));

        if (bIsPlayer)
        {
            BubbleSlot->SetHorizontalAlignment(HAlign_Right);
        }
        else
        {
            BubbleSlot->SetHorizontalAlignment(HAlign_Left);
        }
    }

    // Scroll to bottom
    if (HistoryScrollBox)
    {
        HistoryScrollBox->ScrollToEnd();
    }

    OnMessageAdded();
}
```

### 2.5 ShowChoices 的视觉增强

以下代码替换原有的 `ShowChoices` 方法，使用 `StyleConfig` 统一颜色并添加按钮间距：

```cpp
// DialogueWidget.cpp - 修改 ShowChoices

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
        UButton* ChoiceButton = NewObject<UButton>(ChoicesBox);

        // === 按钮背景样式（使用 StyleConfig 的 AccentColor） ===
        FSlateBrush NormalBrush;
        NormalBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        NormalBrush.TintColor = FLinearColor(0.15f, 0.12f, 0.08f, 0.9f);
        NormalBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius, StyleConfig.BorderRadius);
        NormalBrush.OutlineSettings.Color = FLinearColor(
            StyleConfig.AccentColor.R * 0.5f,
            StyleConfig.AccentColor.G * 0.5f,
            StyleConfig.AccentColor.B * 0.5f, 0.7f);
        NormalBrush.OutlineSettings.Width = 1.0f;
        ChoiceButton->SetBackgroundBrush(NormalBrush);

        // === Hover 效果样式（在蓝图动画或按钮样式中进一步增强） ===
        FSlateBrush HoverBrush;
        HoverBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        HoverBrush.TintColor = FLinearColor(0.25f, 0.2f, 0.12f, 0.95f);
        HoverBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius, StyleConfig.BorderRadius);
        HoverBrush.OutlineSettings.Color = StyleConfig.AccentColor;
        HoverBrush.OutlineSettings.Width = 2.0f;
        ChoiceButton->SetHoverBrush(HoverBrush);

        // 按钮内边距
        ChoiceButton->SetPadding(FMargin(16.0f, 10.0f, 16.0f, 10.0f));

        // 按钮文本
        UTextBlock* ChoiceText = NewObject<UTextBlock>(ChoiceButton);
        ChoiceText->SetText(FText::FromString(FString::Printf(TEXT("%d. %s"), i + 1, *Choices[i])));
        ChoiceText->SetColorAndOpacity(FLinearColor(0.95f, 0.95f, 1.0f, 1.0f));
        FSlateFontInfo ChoiceFont = ChoiceText->GetFont();
        ChoiceFont.Size = 15;
        ChoiceText->SetFont(ChoiceFont);
        ChoiceText->SetAutoWrapText(true);

        ChoiceButton->AddChild(ChoiceText);

        // 添加按钮间距
        UVerticalBoxSlot* ChoiceSlot = Cast<UVerticalBoxSlot>(ChoicesBox->AddChild(ChoiceButton));
        if (ChoiceSlot)
        {
            ChoiceSlot->SetPadding(FMargin(4.0f, 4.0f));
        }

        // 绑定点击事件
        const int32 Index = i;
        TSharedRef<SWidget> WidgetRef = ChoiceButton->TakeWidget();
        TSharedRef<SButton> SlateButtonRef = StaticCastSharedRef<SButton>(WidgetRef);
        SlateButtonRef->SetOnClicked(FOnClicked::CreateWeakLambda(this, [this, Index]() -> FReply
        {
            SelectChoice(Index);
            return FReply::Handled();
        }));
    }

    ShowInputField();

    if (ContinueButton)
    {
        ContinueButton->SetVisibility(ESlateVisibility::Collapsed);
    }
}
```

---

## 3. WorldSetupWidget 美术优化

### 3.1 推荐新增 BindWidget 成员

在 `WorldSetupWidget.h` 中新增以下属性：

```cpp
class UBorder;
class UTextBlock;

// ... 在 UWorldSetupWidget 类的 protected 段新增 ...

    /** Background border panel */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UBorder> BackgroundBorder;

    /** Page title text */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

    /** Description label for world prompt */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DescriptionLabel;

    /** Character name label */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CharacterNameLabel;
```

### 3.2 定义 WorldSetup 风格配置

在 `WorldSetupWidget.h` 文件顶部（类声明之前）定义：

```cpp
USTRUCT(BlueprintType)
struct FWorldSetupStyleConfig
{
    GENERATED_BODY()

    // 背景色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor BackgroundColor = FLinearColor(0.06f, 0.06f, 0.1f, 0.92f);

    // 标题颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor TitleColor = FLinearColor(1.0f, 0.9f, 0.7f, 1.0f);

    // 标签颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor LabelColor = FLinearColor(0.75f, 0.75f, 0.8f, 1.0f);

    // 输入框背景色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor InputBgColor = FLinearColor(0.1f, 0.1f, 0.15f, 0.9f);

    // 按钮背景色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor ButtonBgColor = FLinearColor(0.3f, 0.22f, 0.1f, 1.0f);

    // 按钮边框色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor ButtonBorderColor = FLinearColor(0.8f, 0.65f, 0.3f, 1.0f);

    // 按钮高亮色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor ButtonHoverColor = FLinearColor(0.45f, 0.35f, 0.15f, 1.0f);

    // 主色调
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor AccentColor = FLinearColor(0.5f, 0.4f, 0.2f, 1.0f);

    // 标题字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 TitleFontSize = 32;

    // 标签字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 LabelFontSize = 14;

    // 输入框字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 InputFontSize = 16;

    // 按钮字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 ButtonFontSize = 20;

    // 边框圆角
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    float BorderRadius = 10.0f;
};
```

在 `UWorldSetupWidget` 类中添加：

```cpp
    /** WorldSetup style configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Style")
    FWorldSetupStyleConfig StyleConfig;
```

### 3.3 NativeConstruct 样式增强

以下代码替换 `WorldSetupWidget.cpp` 中的 `NativeConstruct`：

```cpp
// WorldSetupWidget.cpp
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"

void UWorldSetupWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // === 背景面板设置 ===
    if (BackgroundBorder)
    {
        FSlateBrush BgBrush;
        BgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        BgBrush.TintColor = StyleConfig.BackgroundColor;
        BgBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius * 2.0f, StyleConfig.BorderRadius * 2.0f);
        BgBrush.OutlineSettings.Color = FLinearColor(
            StyleConfig.AccentColor.R, StyleConfig.AccentColor.G,
            StyleConfig.AccentColor.B, 0.5f);
        BgBrush.OutlineSettings.Width = 1.5f;
        BackgroundBorder->SetBrush(BgBrush);
    }

    // === 标题文本设置 ===
    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("世界创建")));
        FSlateFontInfo TitleFont = TitleText->GetFont();
        TitleFont.Size = StyleConfig.TitleFontSize;
        TitleFont.OutlineSettings.OutlineSize = 2;
        TitleFont.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.8f);
        TitleText->SetFont(TitleFont);
        TitleText->SetColorAndOpacity(StyleConfig.TitleColor);

        // 发光效果
        TitleText->SetShadowOffset(FVector2D(0.0f, 0.0f));
        TitleText->SetShadowColorAndOpacity(FLinearColor(
            StyleConfig.TitleColor.R, StyleConfig.TitleColor.G,
            StyleConfig.TitleColor.B, 0.35f));
    }

    // === 描述标签设置 ===
    if (DescriptionLabel)
    {
        DescriptionLabel->SetText(FText::FromString(TEXT("描述你的世界")));
        FSlateFontInfo DescFont = DescriptionLabel->GetFont();
        DescFont.Size = StyleConfig.LabelFontSize;
        DescriptionLabel->SetFont(DescFont);
        DescriptionLabel->SetColorAndOpacity(StyleConfig.LabelColor);
    }

    // === 角色名称标签设置 ===
    if (CharacterNameLabel)
    {
        CharacterNameLabel->SetText(FText::FromString(TEXT("角色名称")));
        FSlateFontInfo NameFont = CharacterNameLabel->GetFont();
        NameFont.Size = StyleConfig.LabelFontSize;
        CharacterNameLabel->SetFont(NameFont);
        CharacterNameLabel->SetColorAndOpacity(StyleConfig.LabelColor);
    }

    // === 输入框统一样式 ===
    TArray<UEditableTextBox*> TextBoxes = { WorldPromptTextBox, PlayerCharacterNameBox };
    for (UEditableTextBox* TextBox : TextBoxes)
    {
        if (!TextBox)
        {
            continue;
        }

        FSlateBrush BgBrush;
        BgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        BgBrush.TintColor = StyleConfig.InputBgColor;
        BgBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius, StyleConfig.BorderRadius);
        BgBrush.OutlineSettings.Color = FLinearColor(
            StyleConfig.AccentColor.R * 0.4f,
            StyleConfig.AccentColor.G * 0.4f,
            StyleConfig.AccentColor.B * 0.4f, 0.6f);
        BgBrush.OutlineSettings.Width = 1.0f;
        TextBox->SetBackgroundBrush(BgBrush);

        FSlateFontInfo InputFont = TextBox->GetFont();
        InputFont.Size = StyleConfig.InputFontSize;
        TextBox->SetFont(InputFont);
        TextBox->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.95f, 1.0f));
        TextBox->SetPadding(FMargin(12.0f, 8.0f));
    }

    // === 按钮样式增强 ===
    if (StartGameButton)
    {
        StartGameButton->OnClicked.AddDynamic(this, &UWorldSetupWidget::OnStartGameClicked);

        // Normal state
        FSlateBrush NormalBrush;
        NormalBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        NormalBrush.TintColor = StyleConfig.ButtonBgColor;
        NormalBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius, StyleConfig.BorderRadius);
        NormalBrush.OutlineSettings.Color = StyleConfig.ButtonBorderColor;
        NormalBrush.OutlineSettings.Width = 2.0f;
        StartGameButton->SetBackgroundBrush(NormalBrush);

        // Hover state
        FSlateBrush HoverBrush;
        HoverBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        HoverBrush.TintColor = StyleConfig.ButtonHoverColor;
        HoverBrush.OutlineSettings.CornerRadii = FVector2D(StyleConfig.BorderRadius, StyleConfig.BorderRadius);
        HoverBrush.OutlineSettings.Color = FLinearColor(1.0f, 0.9f, 0.5f, 1.0f);
        HoverBrush.OutlineSettings.Width = 2.5f;
        StartGameButton->SetHoverBrush(HoverBrush);

        StartGameButton->SetPadding(FMargin(28.0f, 14.0f));
    }

    // === 下拉框样式说明 ===
    // UComboBoxString 在 C++ 中可设置的样式选项有限，主要可通过以下方式设置：
    // 1. C++ 端：设置 Font 和 Color
    if (GameStyleSelector)
    {
        GameStyleSelector->AddOption(TEXT("Medieval Fantasy"));
        GameStyleSelector->AddOption(TEXT("Sci-Fi Future"));
        GameStyleSelector->AddOption(TEXT("Modern Horror"));
        GameStyleSelector->AddOption(TEXT("Post-Apocalyptic"));
        GameStyleSelector->AddOption(TEXT("Custom"));
        GameStyleSelector->SetSelectedOption(TEXT("Medieval Fantasy"));

        // 2. 推荐在蓝图端通过 WidgetStyle 属性设置下拉框的完整样式
        //    （包括下拉菜单背景、选中高亮、箭头图标等）
        //    在蓝图编辑器中选中 GameStyleSelector，找到 Widget Style 分类进行设置
    }
}
```

---

## 4. LoadingWidget 美术优化

### 4.1 推荐新增 BindWidget 成员

在 `LoadingWidget.h` 中新增以下属性：

```cpp
class UBorder;
class UCircularThrobber;

// ... 在 ULoadingWidget 类的 protected 段新增 ...

    /** Background border for full-screen semi-transparent overlay */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UBorder> BackgroundBorder;

    /** Animated circular progress indicator */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UCircularThrobber> ProgressThrobber;
```

### 4.2 定义 Loading 风格配置

在 `LoadingWidget.h` 文件顶部（类声明之前）定义：

```cpp
USTRUCT(BlueprintType)
struct FLoadingStyleConfig
{
    GENERATED_BODY()

    // 背景色（全屏半透明覆盖）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.06f, 0.95f);

    // 加载文字颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor LoadingTextColor = FLinearColor(0.95f, 0.95f, 1.0f, 1.0f);

    // 动画点颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor DotsColor = FLinearColor(0.3f, 0.7f, 1.0f, 1.0f);

    // 进度环颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
    FLinearColor ThrobberColor = FLinearColor(0.3f, 0.6f, 1.0f, 1.0f);

    // 加载文字字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 LoadingTextFontSize = 34;

    // 动画点字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fonts")
    int32 DotsFontSize = 48;

    // 文字发光强度（0.0 - 1.0）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float TextGlowIntensity = 0.5f;
};
```

在 `ULoadingWidget` 类中添加：

```cpp
    /** Loading style configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Style")
    FLoadingStyleConfig StyleConfig;
```

### 4.3 样式增强代码

以下代码替换 `LoadingWidget.cpp` 中的 `NativeConstruct`：

```cpp
// LoadingWidget.cpp
#include "Components/Border.h"
#include "Components/CircularThrobber.h"
#include "Styling/SlateBrush.h"

void ULoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // === 背景全屏半透明覆盖 ===
    if (BackgroundBorder)
    {
        FSlateBrush BgBrush;
        BgBrush.DrawAs = ESlateBrushDrawType::Box;
        BgBrush.TintColor = StyleConfig.BackgroundColor;
        BackgroundBorder->SetBrush(BgBrush);
    }

    // === 加载文字居中 + 字体增强 + 发光效果 ===
    if (LoadingText)
    {
        LoadingText->SetText(FText::FromString(BaseLoadingText));

        FSlateFontInfo FontInfo = LoadingText->GetFont();
        FontInfo.Size = StyleConfig.LoadingTextFontSize;
        FontInfo.OutlineSettings.OutlineSize = 2;
        FontInfo.OutlineSettings.OutlineColor = FLinearColor(
            StyleConfig.DotsColor.R * 0.5f,
            StyleConfig.DotsColor.G * 0.5f,
            StyleConfig.DotsColor.B * 0.5f, 0.6f);
        LoadingText->SetFont(FontInfo);

        LoadingText->SetColorAndOpacity(StyleConfig.LoadingTextColor);
        LoadingText->SetAutoWrapText(true);

        // 发光效果（通过零偏移 Shadow 叠加半透明同色光）
        LoadingText->SetShadowOffset(FVector2D(0.0f, 0.0f));
        LoadingText->SetShadowColorAndOpacity(FLinearColor(
            StyleConfig.DotsColor.R,
            StyleConfig.DotsColor.G,
            StyleConfig.DotsColor.B,
            StyleConfig.TextGlowIntensity));
    }

    // === 点动画样式 ===
    if (DotsText)
    {
        FSlateFontInfo FontInfo = DotsText->GetFont();
        FontInfo.Size = StyleConfig.DotsFontSize;
        FontInfo.OutlineSettings.OutlineSize = 1;
        FontInfo.OutlineSettings.OutlineColor = FLinearColor(
            StyleConfig.DotsColor.R * 0.4f,
            StyleConfig.DotsColor.G * 0.4f,
            StyleConfig.DotsColor.B * 0.4f, 0.5f);
        DotsText->SetFont(FontInfo);

        DotsText->SetColorAndOpacity(StyleConfig.DotsColor);

        // 柔和辉光
        DotsText->SetShadowOffset(FVector2D(0.0f, 0.0f));
        DotsText->SetShadowColorAndOpacity(FLinearColor(
            StyleConfig.DotsColor.R,
            StyleConfig.DotsColor.G,
            StyleConfig.DotsColor.B, 0.35f));
    }

    // === 进度环指示器 ===
    if (ProgressThrobber)
    {
        ProgressThrobber->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ProgressThrobber->SetColorAndOpacity(StyleConfig.ThrobberColor);
    }

    UpdateDotsText();
}
```

> **关于点动画的优化建议**：当前实现使用 `NativeTick` 中的定时器控制文字变化。如需更优雅的动画效果，推荐以下方案：
> 1. 在 UMG 蓝图的 Animations 面板中创建名为 `DotBounce` 的循环动画
> 2. 为 `DotsText` 添加缩放关键帧（Scale: 1.0 -> 0.5 -> 1.0，时长 0.4 秒）
> 3. 在 C++ 中通过 `PlayAnimation` 播放，可减少 Tick 中的逻辑开销
> 4. 保留 Tick 逻辑作为蓝图的备选方案

---

## 5. 蓝图端配合修改说明

由于 UMG C++ API 的能力有限，以下效果必须在蓝图编辑器中完成：

### 5.1 磨砂玻璃/毛玻璃效果

C++ 端只能设置简单的半透明颜色。真正的磨砂玻璃效果需要：

1. 在 Content Browser 中创建一个 Material（`M_FrostedGlass`）
2. 材质中使用 Scene Texture 节点 + 模糊后处理实现背景模糊
3. 在 UMG 蓝图中添加 `UImage` 组件，将 Brush 设置为该 Material
4. 或使用 `UBorder` 的 Brush 属性，将 Background Brush 指定到该 Material

### 5.2 按钮悬停/按下动画

C++ 端可以通过 `SetHoverBrush` / `SetPressedBrush` 设置静态颜色变化。如需平滑过渡动画（如缩放、位移、颜色渐变），需要在蓝图动画编辑器中完成：

1. 在 Widget Blueprint 的 Animations 面板创建动画
2. 添加关键帧（如 Scale X/Y 从 1.0 到 1.05）
3. 在按钮的 `OnHovered` / `OnUnhovered` 事件中通过 `Play Animation` 播放

### 5.3 圆角边框材质

C++ 端通过 `FSlateBrush::OutlineSettings` 设置了基础圆角。如需更精细的效果（如渐变边框、发光边框）：

1. 创建一个 Material 实现圆角遮罩
2. 使用材质参数控制圆角大小、边框颜色
3. 在蓝图中将 Border 的 Brush 设置为该 Material

### 5.4 渐变背景

C++ 端的 `FSlateBrush` 不直接支持渐变色。实现渐变背景的方案：

1. 创建一个 Material，使用 Linear Interpolate 或 Gradient 节点
2. 将渐变颜色通过材质参数暴露
3. 在 UMG 蓝图中使用 `UImage` 组件加载该 Material

### 5.5 输入框的内边距和样式

C++ 端可以通过 `SetPadding` 和 `SetBackgroundBrush` 设置基本样式。更完整的输入框样式（焦点高亮、光标颜色、选中颜色）推荐：

1. 在蓝图编辑器中选中 `UEditableTextBox`
2. 在 Widget Style 分类中创建或选择一个 `EditableTextBoxStyle`
3. 配置 Normal、Hovered、Focused、ReadOnly 等各状态的样式
4. 配置 Selection Color、Selected Text Color、Caret Color 等细节

### 5.6 ComboBoxString 样式

`UComboBoxString` 的下拉菜单样式（包括下拉箭头、菜单背景、选中高亮等）完全通过蓝图端设置：

1. 在蓝图编辑器中选中 `GameStyleSelector`
2. 在 Widget Style 分类中创建或选择 `ComboBoxStyle`
3. 分别配置 Normal、Hovered、Pressed 等按钮状态
4. 配置 Menu Border Brush（下拉菜单背景边框）
5. 配置 DropDownArrow Brush（下拉箭头图标）

---

## 6. 关键注意事项

### 6.1 FSlateFontInfo 使用

```cpp
// 正确：通过 GetFont() 获取现有字体，再修改 Size
FSlateFontInfo FontInfo = TextBlock->GetFont();
FontInfo.Size = 24;
TextBlock->SetFont(FontInfo);

// 错误：直接构造空 FSlateFontInfo（会丢失默认字体信息）
FSlateFontInfo FontInfo;  // 避免这种用法
```

### 6.2 委托绑定

```cpp
// 按钮点击必须使用 AddDynamic
SendButton->OnClicked.AddDynamic(this, &UDialogueWidget::OnSendClicked);

// 多播委托必须使用 Broadcast() 调用
OnChoiceSelectedDelegate.Broadcast(ChoiceIndex);
OnMessageSubmittedDelegate.Broadcast(InputText);
OnDialogueClosedDelegate.Broadcast();
```

### 6.3 UMG 颜色设置

```cpp
// SetColorAndOpacity 接受 FLinearColor
TextBlock->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.5f, 1.0f));

// 不能使用 FColor（Srgba 颜色）
// TextBlock->SetColorAndOpacity(FColor::White);  // 错误
```

### 6.4 指针类型

```cpp
// 使用 TObjectPtr 替代裸指针
UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
TObjectPtr<UBorder> BackgroundBorder;  // 正确

// UPROPERTY() 中的裸指针
UBorder* BackgroundBorder;  // 错误（UE5 规范要求使用 TObjectPtr）
```

### 6.5 BindWidgetOptional

```cpp
// 必须绑定的控件使用 BindWidget（蓝图中必须存在同名控件）
UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
TObjectPtr<UButton> StartGameButton;

// 可选绑定的控件使用 BindWidgetOptional（蓝图中可以不存在）
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> TitleText;
```

### 6.6 结构体标记

```cpp
// 需要在蓝图中访问的结构体必须标记为 USTRUCT(BlueprintType)
USTRUCT(BlueprintType)
struct FUIStyleConfig
{
    GENERATED_BODY()
    // ...
};
```

### 6.7 FMargin 参数顺序

```cpp
// SetPadding 参数顺序：Left, Top, Right, Bottom
Slot->SetPadding(FMargin(8.0f, 6.0f, 8.0f, 2.0f));

// 也可以使用命名构造函数提高可读性
Slot->SetPadding(FMargin(8.0f /*Left*/, 6.0f /*Top*/, 8.0f /*Right*/, 2.0f /*Bottom*/));
```

### 6.8 FString 与 TEXT() 宏

```cpp
// 所有字符串字面量必须使用 TEXT() 宏
FString Name = TEXT("NPC_Name");
ThinkingText->SetText(FText::FromString(TEXT("思考中")));

// 错误：不使用 TEXT() 宏（窄字符编码问题）
// FString Name = "NPC_Name";
```

### 6.9 避免原始字符串语法

```cpp
// 正确：使用转义或 TEXT()
FString Path = TEXT("C:\\Game\\Content\\UI");

// 错误：不要使用 LR"()" 原始字符串语法
// FString Path = LR"(C:\Game\Content\UI)";
```

### 6.10 UPROPERTY 类型支持反射

```cpp
// 正确：支持的反射类型
UPROPERTY(EditAnywhere)
FLinearColor MyColor;

UPROPERTY(EditAnywhere)
int32 MyValue;

UPROPERTY(EditAnywhere)
FString MyString;

UPROPERTY(EditAnywhere)
TArray<FString> MyArray;

UPROPERTY(EditAnywhere)
FUIStyleConfig StyleConfig;  // 自定义 USTRUCT(BlueprintType)

// 正确：TObjectPtr 指针类型
UPROPERTY(EditAnywhere)
TObjectPtr<UTexture2D> MyTexture;
```
