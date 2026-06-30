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
    UPROPERTY(EditAnywhere