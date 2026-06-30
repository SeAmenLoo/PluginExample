# UI 风格动态替换逻辑设计文档

## 1. 设计目标

- 支持运行时根据游戏世界观（World Style）动态切换三套 Widget 的 UI 风格
- 风格切换时无需销毁/重建 Widget，直接更新样式
- 风格配置数据化，支持新增/扩展风格
- 与现有代码的 GameStyleSelector（ComboBoxString，选项：Medieval Fantasy / Sci-Fi Future / Modern Horror / Post-Apocalyptic / Custom）联动

## 2. 架构设计

### 2.1 整体流程

```
WorldSetupWidget 选择 GameStyle
        |
        v
广播 OnWorldSetupComplete (携带 WorldPrompt)
        |
        v
GameInstance / UUIStyleSubsystem 收到后，调用 SetActiveStyle 设置当前活跃风格
        |
        v
后续创建的 Widget（DialogueWidget / LoadingWidget / WorldSetupWidget）
读取当前风格配置并应用样式 → ApplyUIStyle()
```

### 2.2 风格数据表 (UDataTable)

使用 UE5 的 `UDataTable` + `USTRUCT` 行结构体来存储多套风格配置。美术/策划可以在编辑器中通过数据表编辑风格，不需要改代码。

**新建文件：`Source/PluginExample/Public/UIStyleTableRow.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UIStyleTableRow.generated.h"

// 单个 UI 风格行的数据
USTRUCT(BlueprintType)
struct FUIStyleTableRow : public FTableRowBase
{
    GENERATED_BODY();

    // ── 风格名称（用于匹配 GameStyle 字符串，如 "Medieval Fantasy"）──
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StyleName;

    // ══════════════ 颜色配置 ══════════════

    // 主背景色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BackgroundColor;

    // 面板/卡片背景色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor PanelBackgroundColor;

    // NPC名称颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor NPCNameColor;

    // 对话文本颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor DialogueTextColor;

    // 玩家消息颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor PlayerMessageColor;

    // NPC消息颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor NPCMessageColor;

    // 输入框背景色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor InputBackgroundColor;

    // 思考指示器颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ThinkingColor;

    // 主色调/强调色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor AccentColor;

    // 按钮正常状态色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ButtonNormalColor;

    // 按钮悬停状态色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ButtonHoverColor;

    // 按钮按下状态色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ButtonPressedColor;

    // 标题文字颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TitleTextColor;

    // 正文文字颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BodyTextColor;

    // 加载动画颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor LoadingAnimColor;

    // 边框颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BorderColor;

    // 边框发光色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BorderGlowColor;

    // ══════════════ 字体配置 ══════════════

    // 字体族名称（空字符串表示使用默认字体）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString FontFamilyName;

    // NPC名称字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NPCNameFontSize = 28;

    // 对话文本字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DialogueTextFontSize = 20;

    // 消息说话者字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MessageSpeakerFontSize = 16;

    // 消息正文字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MessageTextFontSize = 14;

    // 标题字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TitleFontSize = 36;

    // 加载文字字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LoadingTextFontSize = 32;

    // 按钮文字字号
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ButtonTextFontSize = 18;

    // ══════════════ 布局/形状配置 ══════════════

    // 圆角大小
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BorderRadius = 8.0f;

    // 面板内边距
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PanelPadding = 16.0f;

    // 消息气泡内边距
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BubblePadding = 12.0f;

    // 消息间距
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MessageSpacing = 8.0f;

    // 透明度（整体UI透明度）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallOpacity = 0.85f;
};
```

### 2.3 风格管理子系统 (UUIStyleSubsystem)

创建一个 GameInstanceSubsystem 来全局管理当前活跃风格。作为 Subsystem，它随 GameInstance 生命周期存在，所有 Widget 都能访问到。

**新建文件：`Source/PluginExample/Public/UIStyleSubsystem.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UIStyleTableRow.h"
#include "UIStyleSubsystem.generated.h"

class UDialogueWidget;
class ULoadingWidget;
class UWorldSetupWidget;

UCLASS()
class PLUGINEXAMPLE_API UUIStyleSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 设置当前活跃的风格（通过名称匹配 DataTable 中的 StyleName） */
    UFUNCTION(BlueprintCallable, Category = "UI Style")
    void SetActiveStyle(const FString& StyleName);

    /** 获取当前活跃的风格配置，未设置时返回 nullptr */
    UFUNCTION(BlueprintPure, Category = "UI Style")
    const FUIStyleTableRow* GetActiveStyle() const;

    /** 根据 FName 键名获取对应颜色（用于蓝图快捷访问） */
    UFUNCTION(BlueprintPure, Category = "UI Style")
    FLinearColor GetColor(FName ColorKey) const;

    /** 检查是否已设置活跃风格 */
    UFUNCTION(BlueprintPure, Category = "UI Style")
    bool HasActiveStyle() const;

    /** 获取当前风格名称 */
    UFUNCTION(BlueprintPure, Category = "UI Style")
    FString GetActiveStyleName() const;

    /** 将当前风格应用到 DialogueWidget */
    UFUNCTION(BlueprintCallable, Category = "UI Style")
    void ApplyStyleToDialogue(class UDialogueWidget* Widget);

    /** 将当前风格应用到 LoadingWidget */
    UFUNCTION(BlueprintCallable, Category = "UI Style")
    void ApplyStyleToLoading(class ULoadingWidget* Widget);

    /** 将当前风格应用到 WorldSetupWidget */
    UFUNCTION(BlueprintCallable, Category = "UI Style")
    void ApplyStyleToWorldSetup(class UWorldSetupWidget* Widget);

private:
    // 当前激活的风格名称
    UPROPERTY()
    FString CurrentStyleName;

    // 风格数据表引用
    UPROPERTY()
    TObjectPtr<UDataTable> StyleDataTable;

    // 缓存的当前风格行数据（原始指针，指向 DataTable 内部数据，不需要手动释放）
    const FUIStyleTableRow* CachedStyleRow;
};
```

**新建文件：`Source/PluginExample/Private/UIStyleSubsystem.cpp`**

```cpp
#include "UIStyleSubsystem.h"
#include "DialogueWidget.h"
#include "LoadingWidget.h"
#include "WorldSetupWidget.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"

void UUIStyleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 加载风格数据表（硬编码路径，需确保 Content/UI/DT_UIStyles.uasset 已创建）
    static const FString DataTablePath = TEXT("/Game/UI/DT_UIStyles.DT_UIStyles");
    StyleDataTable = LoadObject<UDataTable>(nullptr, *DataTablePath, nullptr, LOAD_None, nullptr);

    if (!StyleDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("UUIStyleSubsystem: Failed to load DataTable at '%s'"), *DataTablePath);
    }

    CachedStyleRow = nullptr;
    CurrentStyleName.Empty();

    // 设置默认风格
    SetActiveStyle(TEXT("Medieval Fantasy"));
}

void UUIStyleSubsystem::Deinitialize()
{
    Super::Deinitialize();
    CachedStyleRow = nullptr;
}

void UUIStyleSubsystem::SetActiveStyle(const FString& StyleName)
{
    if (!StyleDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("UUIStyleSubsystem: StyleDataTable is null, cannot set style"));
        return;
    }

    CurrentStyleName = StyleName;

    // 在 DataTable 中查找对应行
    static const FName StyleNameKey = TEXT("StyleName");
    TArray<FUIStyleTableRow*> AllRows;
    StyleDataTable->GetAllRows<FUIStyleTableRow>(TEXT("UUIStyleSubsystem::SetActiveStyle"), AllRows);

    CachedStyleRow = nullptr;
    for (FUIStyleTableRow* Row : AllRows)
    {
        if (Row && Row->StyleName == StyleName)
        {
            CachedStyleRow = Row;
            UE_LOG(LogTemp, Log, TEXT("UUIStyleSubsystem: Active style set to '%s'"), *StyleName);
            break;
        }
    }

    if (!CachedStyleRow)
    {
        UE_LOG(LogTemp, Warning, TEXT("UUIStyleSubsystem: Style '%s' not found in DataTable"), *StyleName);
    }
}

const FUIStyleTableRow* UUIStyleSubsystem::GetActiveStyle() const
{
    return CachedStyleRow;
}

FLinearColor UUIStyleSubsystem::GetColor(FName ColorKey) const
{
    if (!CachedStyleRow)
    {
        return FLinearColor::White;
    }

    if (ColorKey == TEXT("BackgroundColor"))        return CachedStyleRow->BackgroundColor;
    if (ColorKey == TEXT("PanelBackgroundColor"))   return CachedStyleRow->PanelBackgroundColor;
    if (ColorKey == TEXT("NPCNameColor"))            return CachedStyleRow->NPCNameColor;
    if (ColorKey == TEXT("DialogueTextColor"))       return CachedStyleRow->DialogueTextColor;
    if (ColorKey == TEXT("PlayerMessageColor"))      return CachedStyleRow->PlayerMessageColor;
    if (ColorKey == TEXT("NPCMessageColor"))         return CachedStyleRow->NPCMessageColor;
    if (ColorKey == TEXT("InputBackgroundColor"))    return CachedStyleRow->InputBackgroundColor;
    if (ColorKey == TEXT("ThinkingColor"))           return CachedStyleRow->ThinkingColor;
    if (ColorKey == TEXT("AccentColor"))             return CachedStyleRow->AccentColor;
    if (ColorKey == TEXT("ButtonNormalColor"))       return CachedStyleRow->ButtonNormalColor;
    if (ColorKey == TEXT("ButtonHoverColor"))        return CachedStyleRow->ButtonHoverColor;
    if (ColorKey == TEXT("ButtonPressedColor"))      return CachedStyleRow->ButtonPressedColor;
    if (ColorKey == TEXT("TitleTextColor"))          return CachedStyleRow->TitleTextColor;
    if (ColorKey == TEXT("BodyTextColor"))           return CachedStyleRow->BodyTextColor;
    if (ColorKey == TEXT("LoadingAnimColor"))        return CachedStyleRow->LoadingAnimColor;
    if (ColorKey == TEXT("BorderColor"))             return CachedStyleRow->BorderColor;
    if (ColorKey == TEXT("BorderGlowColor"))         return CachedStyleRow->BorderGlowColor;

    UE_LOG(LogTemp, Warning, TEXT("UUIStyleSubsystem::GetColor: Unknown ColorKey '%s'"), *ColorKey.ToString());
    return FLinearColor::White;
}

bool UUIStyleSubsystem::HasActiveStyle() const
{
    return CachedStyleRow != nullptr;
}

FString UUIStyleSubsystem::GetActiveStyleName() const
{
    return CurrentStyleName;
}

void UUIStyleSubsystem::ApplyStyleToDialogue(class UDialogueWidget* Widget)
{
    if (!Widget || !CachedStyleRow)
    {
        return;
    }
    Widget->ApplyUIStyle(*CachedStyleRow);
}

void UUIStyleSubsystem::ApplyStyleToLoading(class ULoadingWidget* Widget)
{
    if (!Widget || !CachedStyleRow)
    {
        return;
    }
    Widget->ApplyUIStyle(*CachedStyleRow);
}

void UUIStyleSubsystem::ApplyStyleToWorldSetup(class UWorldSetupWidget* Widget)
{
    if (!Widget || !CachedStyleRow)
    {
        return;
    }
    Widget->ApplyUIStyle(*CachedStyleRow);
}
```

### 2.4 各 Widget 的风格应用方法

在每个 Widget 中添加一个 `ApplyUIStyle(const FUIStyleTableRow& Style)` 方法，接受风格行数据参数，更新所有颜色、字体大小、背景色等样式属性。

#### 2.4.1 DialogueWidget

**在 `DialogueWidget.h` 中新增：**

```cpp
// 头文件新增前向声明
struct FUIStyleTableRow;

// 在 public 段新增方法声明
public:
    /** 应用 UI 风格配置到所有 UI 元素 */
    UFUNCTION(BlueprintCallable, Category = "UI Style")
    void ApplyUIStyle(const FUIStyleTableRow& Style);
```

**在 `DialogueWidget.cpp` 中新增实现：**

```cpp
#include "UIStyleTableRow.h"

void UDialogueWidget::ApplyUIStyle(const FUIStyleTableRow& Style)
{
    // ── NPC 名称 ──
    if (NPCNameText)
    {
        NPCNameText->SetColorAndOpacity(Style.NPCNameColor);
        FSlateFontInfo NameFont = NPCNameText->GetFont();
        NameFont.Size = Style.NPCNameFontSize;
        NPCNameText->SetFont(NameFont);
    }

    // ── 对话文本（NPC 响应区域）──
    if (DialogueText)
    {
        DialogueText->SetColorAndOpacity(Style.DialogueTextColor);
        FSlateFontInfo DialogueFont = DialogueText->GetFont();
        DialogueFont.Size = Style.DialogueTextFontSize;
        DialogueText->SetFont(DialogueFont);
    }

    // ── 思考指示器 ──
    if (ThinkingText)
    {
        ThinkingText->SetColorAndOpacity(Style.ThinkingColor);
    }

    // ── 输入框背景 ──
    if (InputTextBox)
    {
        // 通过 SetForegroundColor 间接设置输入框颜色主题
        InputTextBox->SetForegroundColor(FSlateColor(Style.BodyTextColor));
    }

    // ── 历史消息：需要重建已有消息的样式 ──
    // 由于历史消息是动态创建的子 Widget，
    // 新创建的消息会通过 AddMessageToHistory 使用缓存的风格颜色
    // 此处可以选择清除历史并重新应用（简单方案），
    // 也可以遍历 HistoryContainer 的子元素更新颜色（复杂方案）
    // 推荐在风格切换时清除历史以保持一致性
    if (HistoryContainer)
    {
        HistoryContainer->ClearChildren();
    }

    UE_LOG(LogTemp, Log, TEXT("DialogueWidget: UI style applied '%s'"), *Style.StyleName);
}
```

> **注意：** 为了让 `AddMessageToHistory` 也使用风格颜色，需要在 `DialogueWidget.h` 中新增一个缓存成员，并在 `ApplyUIStyle` 中更新它：

```cpp
// DialogueWidget.h 私有段新增
private:
    // 缓存的风格指针（不拥有所有权，仅用于读取）
    const FUIStyleTableRow* CachedStyle = nullptr;
```

然后修改 `AddMessageToHistory` 中硬编码的颜色为读取 `CachedStyle`：

```cpp
void UDialogueWidget::AddMessageToHistory(const FString& SpeakerName, const FString& Message, bool bIsPlayer)
{
    if (!HistoryContainer)
    {
        return;
    }

    UVerticalBox* MessageEntry = NewObject<UVerticalBox>(HistoryContainer);

    // ── 说话者名称 ──
    UTextBlock* SpeakerText = NewObject<UTextBlock>(MessageEntry);
    SpeakerText->SetText(FText::FromString(bIsPlayer ? TEXT("You") : SpeakerName));

    // 使用风格颜色，若无缓存则使用默认色
    FLinearColor SpeakerColor = bIsPlayer
        ? (CachedStyle ? CachedStyle->PlayerMessageColor : FLinearColor(0.3f, 0.8f, 1.0f))
        : (CachedStyle ? CachedStyle->NPCMessageColor : FLinearColor(1.0f, 0.8f, 0.3f));
    SpeakerText->SetColorAndOpacity(SpeakerColor);

    FSlateFontInfo SpeakerFont = SpeakerText->GetFont();
    SpeakerFont.Size = CachedStyle ? CachedStyle->MessageSpeakerFontSize : 16;
    SpeakerText->SetFont(SpeakerFont);
    SpeakerText->SetAutoWrapText(true);
    MessageEntry->AddChild(SpeakerText);

    // ── 消息正文 ──
    UTextBlock* MessageText = NewObject<UTextBlock>(MessageEntry);
    MessageText->SetText(FText::FromString(Message));
    MessageText->SetColorAndOpacity(CachedStyle ? CachedStyle->BodyTextColor : FLinearColor::White);

    FSlateFontInfo MessageFont = MessageText->GetFont();
    MessageFont.Size = CachedStyle ? CachedStyle->MessageTextFontSize : 14;
    MessageText->SetFont(MessageFont);
    MessageText->SetAutoWrapText(true);
    MessageEntry->AddChild(MessageText);

    HistoryContainer->AddChild(MessageEntry);

    if (HistoryScrollBox)
    {
        HistoryScrollBox->ScrollToEnd();
    }

    OnMessageAdded();
}
```

同时在 `ApplyUIStyle` 末尾更新缓存：

```cpp
void UDialogueWidget::ApplyUIStyle(const FUIStyleTableRow& Style)
{
    // ... 前面的代码不变 ...

    CachedStyle = &Style;  // 缓存风格指针

    UE_LOG(LogTemp, Log, TEXT("DialogueWidget: UI style applied '%s'"), *Style.StyleName);
}
```

#### 2.4.2 LoadingWidget

**在 `LoadingWidget.h` 中新增：**

```cpp
// 头文件新增前向声明
struct FUIStyleTableRow;

// 在 public 段新增方法声明
public:
    /** 应用 UI 风格配置到加载界面元素 */
    UFUNCTION(BlueprintCallable, Category = "UI Style")
    void ApplyUIStyle(const FUIStyleTableRow& Style);
```

**在 `LoadingWidget.cpp` 中新增实现：**

```cpp
#include "UIStyleTableRow.h"

void ULoadingWidget::ApplyUIStyle(const FUIStyleTableRow& Style)
{
    // ── 加载文字 ──
    if (LoadingText)
    {
        LoadingText->SetColorAndOpacity(Style.TitleTextColor);
        FSlateFontInfo FontInfo = LoadingText->GetFont();
        FontInfo.Size = Style.LoadingTextFontSize;
        LoadingText->SetFont(FontInfo);
    }

    // ── 动画圆点 ──
    if (DotsText)
    {
        DotsText->SetColorAndOpacity(Style.LoadingAnimColor);
    }

    UE_LOG(LogTemp, Log, TEXT("LoadingWidget: UI style applied '%s'"), *Style.StyleName);
}
```

#### 2.4.3 WorldSetupWidget

**在 `WorldSetupWidget.h` 中新增：**

```cpp
// 头文件新增前向声明
struct FUIStyleTableRow;
class UTextBlock;
class UBorder;
class UImage;
class UCanvasPanel;

// 在 public 段新增方法声明
public:
    /** 应用 UI 风格配置到世界设置界面元素 */
    UFUNCTION(BlueprintCallable, Category = "UI Style")
    void ApplyUIStyle(const FUIStyleTableRow& Style);

// 在 protected 段新增 BindWidget 成员（根据蓝图实际情况可选绑定）
protected:
    /** 主标题文字（可选绑定） */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

    /** 背景面板 Border（可选绑定） */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BackgroundBorder;

    /** 副标题/描述文字（可选绑定） */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SubtitleText;
```

**在 `WorldSetupWidget.cpp` 中新增实现：**

```cpp
#include "UIStyleTableRow.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

void UWorldSetupWidget::ApplyUIStyle(const FUIStyleTableRow& Style)
{
    // ── 标题文字 ──
    if (TitleText)
    {
        TitleText->SetColorAndOpacity(Style.TitleTextColor);
        FSlateFontInfo TitleFont = TitleText->GetFont();
        TitleFont.Size = Style.TitleFontSize;
        TitleText->SetFont(TitleFont);
    }

    // ── 副标题 ──
    if (SubtitleText)
    {
        SubtitleText->SetColorAndOpacity(Style.BodyTextColor);
    }

    // ── 背景面板 ──
    if (BackgroundBorder)
    {
        BackgroundBorder->SetBrushColor(Style.PanelBackgroundColor);
    }

    // ── 输入框文字颜色 ──
    if (WorldPromptTextBox)
    {
        WorldPromptTextBox->SetForegroundColor(FSlateColor(Style.BodyTextColor));
    }

    if (PlayerCharacterNameBox)
    {
        PlayerCharacterNameBox->SetForegroundColor(FSlateColor(Style.BodyTextColor));
    }

    UE_LOG(LogTemp, Log, TEXT("WorldSetupWidget: UI style applied '%s'"), *Style.StyleName);
}
```

### 2.5 风格切换调用点

说明在哪些位置调用 `SetActiveStyle` 和 `ApplyUIStyle`：

#### 调用点 1：WorldSetupWidget::OnStartGameClicked

在生成 Prompt 之前先设置风格。修改现有的 `OnStartGameClicked`：

```cpp
// WorldSetupWidget.cpp
#include "UIStyleSubsystem.h"
#include "Engine/GameInstance.h"

void UWorldSetupWidget::OnStartGameClicked()
{
    // ── 设置活跃风格 ──
    if (GameStyleSelector)
    {
        FString SelectedStyle = GameStyleSelector->GetSelectedOption();
        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            UUIStyleSubsystem* StyleSubsystem = GI->GetSubsystem<UUIStyleSubsystem>();
            if (StyleSubsystem)
            {
                StyleSubsystem->SetActiveStyle(SelectedStyle);
            }
        }
    }

    // ── 构建 Prompt（原有逻辑不变）──
    FString Prompt = TEXT("Create a ");

    if (GameStyleSelector)
    {
        FString Style = GameStyleSelector->GetSelectedOption();
        Prompt += Style + TEXT(" style ");
    }
    else
    {
        Prompt += TEXT("medieval fantasy ");
    }

    if (WorldPromptTextBox && !WorldPromptTextBox->GetText().IsEmpty())
    {
        Prompt += TEXT("world with the following details: ") + WorldPromptTextBox->GetText().ToString();
    }
    else
    {
        Prompt += TEXT("world with a rich lore, diverse factions, and interesting characters.");
    }

    if (PlayerCharacterNameBox && !PlayerCharacterNameBox->GetText().IsEmpty())
    {
        Prompt += FString::Printf(TEXT(" The player's character name is: %s."), *PlayerCharacterNameBox->GetText().ToString());
    }

    OnWorldSetupComplete.Broadcast(Prompt);
    RemoveFromParent();
}
```

#### 调用点 2：PluginGameInstance::Init

设置默认风格（Medieval Fantasy），确保子系统初始化时已设置。

```cpp
// PluginGameInstance.cpp
#include "UIStyleSubsystem.h"

void UPluginGameInstance::Init()
{
    Super::Init();

    // ── 初始化 UI 风格子系统 ──
    UUIStyleSubsystem* StyleSubsystem = GetSubsystem<UUIStyleSubsystem>();
    if (StyleSubsystem)
    {
        // Subsystem::Initialize 已设置默认 Medieval Fantasy
        // 此处可覆盖或确认
        if (!StyleSubsystem->HasActiveStyle())
        {
            StyleSubsystem->SetActiveStyle(TEXT("Medieval Fantasy"));
        }
    }

    // ... 原有 ChunkManager 初始化逻辑不变 ...
}
```

#### 调用点 3：NPC 对话开始时（BasePlayerController）

在创建 DialogueWidget 后立即应用风格。修改 `BasePlayerController.cpp` 中创建 DialogueWidget 的位置（如果有的话），以及新增一个辅助方法：

```cpp
// BasePlayerController.cpp 中创建 DialogueWidget 的地方（示意）
#include "UIStyleSubsystem.h"
#include "UIStyleTableRow.h"
#include "DialogueWidget.h"

void ABasePlayerController::ShowDialogue()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UUIStyleSubsystem* StyleSubsystem = GI->GetSubsystem<UUIStyleSubsystem>();

    UDialogueWidget* Dialogue = CreateWidget<UDialogueWidget>(this, DialogueWidgetClass);
    if (Dialogue)
    {
        // 应用当前风格
        if (StyleSubsystem && StyleSubsystem->HasActiveStyle())
        {
            StyleSubsystem->ApplyStyleToDialogue(Dialogue);
        }

        Dialogue->AddToViewport(100);
    }
}
```

> **说明：** 当前 `BasePlayerController` 中没有直接创建 `DialogueWidget` 的代码（DialogueWidget 的创建可能通过 NPC 交互触发）。需要找到实际的 DialogueWidget 创建位置（可能在 `NPCCharacter` 或蓝图中），并在创建后添加风格应用逻辑。

#### 调用点 4：加载界面显示时（BasePlayerController::ShowLoadingScreen）

修改现有的 `ShowLoadingScreen` 方法，在创建 LoadingWidget 后应用风格：

```cpp
// BasePlayerController.cpp - 修改 ShowLoadingScreen
void ABasePlayerController::ShowLoadingScreen()
{
    if (LoadingWidget)
    {
        LoadingWidget->RemoveFromParent();
        LoadingWidget = nullptr;
    }

    if (!LoadingWidgetClass && PluginGameInstance)
    {
        UClass* FoundClass = PluginGameInstance->GetClass();
    }

    if (LoadingWidgetClass)
    {
        LoadingWidget = CreateWidget<ULoadingWidget>(this, LoadingWidgetClass);
        if (LoadingWidget)
        {
            LoadingWidget->SetLoadingText(TEXT("Generating world"));
            LoadingWidget->SetAnimatedDots(true);

            // ── 应用当前风格 ──
            UGameInstance* GI = GetGameInstance();
            if (GI)
            {
                UUIStyleSubsystem* StyleSubsystem = GI->GetSubsystem<UUIStyleSubsystem>();
                if (StyleSubsystem && StyleSubsystem->HasActiveStyle())
                {
                    StyleSubsystem->ApplyStyleToLoading(LoadingWidget);
                }
            }

            LoadingWidget->AddToViewport(10);

            FInputModeUIOnly InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            SetInputMode(InputMode);
            bShowMouseCursor = true;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("LoadingWidgetClass not set in BasePlayerController, using text log only"));
    }
}
```

## 3. 预设风格数据表配置

以下 JSON 格式数据可用于导入 UDataTable（也可使用 CSV 格式）。在编辑器中创建 DataTable 后，通过 Import 按钮导入。

### 3.1 Modern Minimal（简约风）

| 字段 | 值 |
|------|----|
| StyleName | Modern Minimal |
| BackgroundColor | (R=0.059, G=0.059, B=0.098, A=0.85) |
| PanelBackgroundColor | (R=0.078, G=0.078, B=0.137, A=0.9) |
| NPCNameColor | (R=1.0, G=0.85, B=0.5, A=1.0) |
| DialogueTextColor | (R=0.95, G=0.95, B=1.0, A=1.0) |
| PlayerMessageColor | (R=0.3, G=0.8, B=1.0, A=1.0) |
| NPCMessageColor | (R=1.0, G=0.8, B=0.3, A=1.0) |
| InputBackgroundColor | (R=0.1, G=0.1, B=0.18, A=0.8) |
| ThinkingColor | (R=0.5, G=0.7, B=1.0, A=1.0) |
| AccentColor | (R=0.3, G=0.6, B=1.0, A=1.0) |
| ButtonNormalColor | (R=0.15, G=0.15, B=0.25, A=1.0) |
| ButtonHoverColor | (R=0.2, G=0.3, B=0.5, A=1.0) |
| ButtonPressedColor | (R=0.1, G=0.2, B=0.4, A=1.0) |
| TitleTextColor | (R=0.95, G=0.95, B=1.0, A=1.0) |
| BodyTextColor | (R=0.85, G=0.85, B=0.9, A=1.0) |
| LoadingAnimColor | (R=0.3, G=0.7, B=1.0, A=1.0) |
| BorderColor | (R=0.2, G=0.2, B=0.3, A=0.5) |
| BorderGlowColor | (R=0.3, G=0.5, B=1.0, A=0.0) |
| FontFamilyName | (空) |
| NPCNameFontSize | 28 |
| DialogueTextFontSize | 20 |
| MessageSpeakerFontSize | 16 |
| MessageTextFontSize | 14 |
| TitleFontSize | 36 |
| LoadingTextFontSize | 32 |
| ButtonTextFontSize | 18 |
| BorderRadius | 8.0 |
| PanelPadding | 16.0 |
| BubblePadding | 12.0 |
| MessageSpacing | 8.0 |
| OverallOpacity | 0.85 |

### 3.2 Medieval Fantasy（中世纪奇幻）

| 字段 | 值 |
|------|----|
| StyleName | Medieval Fantasy |
| BackgroundColor | (R=0.078, G=0.047, B=0.031, A=0.88) |
| PanelBackgroundColor | (R=0.176, G=0.118, B=0.059, A=0.92) |
| NPCNameColor | (R=1.0, G=0.75, B=0.2, A=1.0) |
| DialogueTextColor | (R=0.95, G=0.88, B=0.75, A=1.0) |
| PlayerMessageColor | (R=0.4, G=0.6, B=0.9, A=1.0) |
| NPCMessageColor | (R=1.0, G=0.75, B=0.2, A=1.0) |
| InputBackgroundColor | (R=0.15, G=0.1, B=0.05, A=0.8) |
| ThinkingColor | (R=0.8, G=0.6, B=0.2, A=1.0) |
| AccentColor | (R=0.8, G=0.6, B=0.15, A=1.0) |
| ButtonNormalColor | (R=0.2, G=0.12, B=0.05, A=1.0) |
| ButtonHoverColor | (R=0.3, G=0.2, B=0.1, A=1.0) |
| ButtonPressedColor | (R=0.15, G=0.08, B=0.02, A=1.0) |
| TitleTextColor | (R=1.0, G=0.85, B=0.4, A=1.0) |
| BodyTextColor | (R=0.9, G=0.82, B=0.68, A=1.0) |
| LoadingAnimColor | (R=0.8, G=0.6, B=0.2, A=1.0) |
| BorderColor | (R=0.6, G=0.45, B=0.1, A=0.8) |
| BorderGlowColor | (R=0.8, G=0.6, B=0.15, A=0.3) |
| FontFamilyName | MedievalSharp |
| NPCNameFontSize | 26 |
| DialogueTextFontSize | 19 |
| MessageSpeakerFontSize | 15 |
| MessageTextFontSize | 13 |
| TitleFontSize | 38 |
| LoadingTextFontSize | 30 |
| ButtonTextFontSize | 17 |
| BorderRadius | 4.0 |
| PanelPadding | 20.0 |
| BubblePadding | 14.0 |
| MessageSpacing | 10.0 |
| OverallOpacity | 0.88 |

### 3.3 Sci-Fi Future（科幻风）

| 字段 | 值 |
|------|----|
| StyleName | Sci-Fi Future |
| BackgroundColor | (R=0.02, G=0.02, B=0.078, A=0.9) |
| PanelBackgroundColor | (R=0.039, G=0.059, B=0.157, A=0.92) |
| NPCNameColor | (R=0.0, G=1.0, B=0.9, A=1.0) |
| DialogueTextColor | (R=0.8, G=0.9, B=1.0, A=1.0) |
| PlayerMessageColor | (R=0.2, G=1.0, B=0.4, A=1.0) |
| NPCMessageColor | (R=0.0, G=0.9, B=1.0, A=1.0) |
| InputBackgroundColor | (R=0.05, G=0.05, B=0.12, A=0.8) |
| ThinkingColor | (R=0.0, G=0.8, B=1.0, A=1.0) |
| AccentColor | (R=0.0, G=0.5, B=1.0, A=1.0) |
| ButtonNormalColor | (R=0.05, G=0.1, B=0.2, A=1.0) |
| ButtonHoverColor | (R=0.0, G=0.3, B=0.6, A=1.0) |
| ButtonPressedColor | (R=0.0, G=0.2, B=0.5, A=1.0) |
| TitleTextColor | (R=0.0, G=1.0, B=0.9, A=1.0) |
| BodyTextColor | (R=0.7, G=0.85, B=1.0, A=1.0) |
| LoadingAnimColor | (R=0.0, G=0.5, B=1.0, A=1.0) |
| BorderColor | (R=0.0, G=0.39, B=1.0, A=0.4) |
| BorderGlowColor | (R=0.0, G=0.5, B=1.0, A=0.6) |
| FontFamilyName | Orbitron |
| NPCNameFontSize | 24 |
| DialogueTextFontSize | 18 |
| MessageSpeakerFontSize | 15 |
| MessageTextFontSize | 13 |
| TitleFontSize | 34 |
| LoadingTextFontSize | 28 |
| ButtonTextFontSize | 16 |
| BorderRadius | 2.0 |
| PanelPadding | 12.0 |
| BubblePadding | 10.0 |
| MessageSpacing | 6.0 |
| OverallOpacity | 0.9 |

### 3.4 Modern Horror（现代恐怖）

| 字段 | 值 |
|------|----|
| StyleName | Modern Horror |
| BackgroundColor | (R=0.059, G=0.02, B=0.02, A=0.9) |
| PanelBackgroundColor | (R=0.137, G=0.039, B=0.039, A=0.92) |
| NPCNameColor | (R=0.8, G=0.15, B=0.15, A=1.0) |
| DialogueTextColor | (R=0.75, G=0.7, B=0.7, A=1.0) |
| PlayerMessageColor | (R=0.6, G=0.6, B=0.65, A=1.0) |
| NPCMessageColor | (R=0.8, G=0.15, B=0.15, A=1.0) |
| InputBackgroundColor | (R=0.1, G=0.03, B=0.03, A=0.8) |
| ThinkingColor | (R=0.6, G=0.1, B=0.1, A=1.0) |
| AccentColor | (R=0.6, G=0.1, B=0.1, A=1.0) |
| ButtonNormalColor | (R=0.15, G=0.05, B=0.05, A=1.0) |
| ButtonHoverColor | (R=0.3, G=0.08, B=0.08, A=1.0) |
| ButtonPressedColor | (R=0.2, G=0.05, B=0.05, A=1.0) |
| TitleTextColor | (R=0.9, G=0.2, B=0.2, A=1.0) |
| BodyTextColor | (R=0.7, G=0.65, B=0.65, A=1.0) |
| LoadingAnimColor | (R=0.6, G=0.1, B=0.1, A=1.0) |
| BorderColor | (R=0.4, G=0.05, B=0.05, A=0.8) |
| BorderGlowColor | (R=0.6, G=0.1, B=0.1, A=0.3) |
| FontFamilyName | Creepster |
| NPCNameFontSize | 26 |
| DialogueTextFontSize | 18 |
| MessageSpeakerFontSize | 15 |
| MessageTextFontSize | 13 |
| TitleFontSize | 36 |
| LoadingTextFontSize | 30 |
| ButtonTextFontSize | 17 |
| BorderRadius | 0.0 |
| PanelPadding | 14.0 |
| BubblePadding | 10.0 |
| MessageSpacing | 8.0 |
| OverallOpacity | 0.9 |

### CSV 导入格式示例

如需通过 CSV 导入，可按以下格式准备文件（保存为 `DT_UIStyles.csv`）：

```csv
"StyleName","BackgroundColor","PanelBackgroundColor","NPCNameColor","DialogueTextColor","PlayerMessageColor","NPCMessageColor","InputBackgroundColor","ThinkingColor","AccentColor","ButtonNormalColor","ButtonHoverColor","ButtonPressedColor","TitleTextColor","BodyTextColor","LoadingAnimColor","BorderColor","BorderGlowColor","FontFamilyName","NPCNameFontSize","DialogueTextFontSize","MessageSpeakerFontSize","MessageTextFontSize","TitleFontSize","LoadingTextFontSize","ButtonTextFontSize","BorderRadius","PanelPadding","BubblePadding","MessageSpacing","OverallOpacity"
"Modern Minimal","(R=0.059,G=0.059,B=0.098,A=0.85)","(R=0.078,G=0.078,B=0.137,A=0.9)","(R=1.0,G=0.85,B=0.5,A=1.0)","(R=0.95,G=0.95,B=1.0,A=1.0)","(R=0.3,G=0.8,B=1.0,A=1.0)","(R=1.0,G=0.8,B=0.3,A=1.0)","(R=0.1,G=0.1,B=0.18,A=0.8)","(R=0.5,G=0.7,B=1.0,A=1.0)","(R=0.3,G=0.6,B=1.0,A=1.0)","(R=0.15,G=0.15,B=0.25,A=1.0)","(R=0.2,G=0.3,B=0.5,A=1.0)","(R=0.1,G=0.2,B=0.4,A=1.0)","(R=0.95,G=0.95,B=1.0,A=1.0)","(R=0.85,G=0.85,B=0.9,A=1.0)","(R=0.3,G=0.7,B=1.0,A=1.0)","(R=0.2,G=0.2,B=0.3,A=0.5)","(R=0.3,G=0.5,B=1.0,A=0.0)","",-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
```

> **提示：** CSV 导入中 `int32` 值不能为空，如果使用默认值，需要填入实际数字。`FString` 字段为空时留空即可。更推荐在编辑器中手动添加数据行。

## 4. 推荐的数据表创建步骤

1. 在 Content Browser 中创建 `Content/UI/` 文件夹
2. 右键 → Miscellaneous → Data Table
3. 选择行结构体为 `FUIStyleTableRow`
4. 命名为 `DT_UIStyles`
5. 双击打开，点击 "Add Row" 添加 4 行数据：
   - `Modern Minimal`
   - `Medieval Fantasy`
   - `Sci-Fi Future`
   - `Modern Horror`
6. 参照第 3 节的数值填入每行数据
7. 在 `UIStyleSubsystem::Initialize` 中通过 `LoadObject<UDataTable>` 加载该资源

> **Build.cs 修改：** `UGameInstanceSubsystem` 所在的 `Subsystems` 模块已包含在 `Engine` 中，无需额外添加依赖。但 `UDataTable` 所需的 `Engine` 模块已在现有 Build.cs 中声明，所以无需修改 `PluginExample.Build.cs`。

## 5. Widget 蓝图端的风格响应

说明如何在 Widget Blueprint 中配合 C++ 风格切换系统实现更丰富的视觉效果：

### 5.1 颜色过渡动画

- 在 Widget Blueprint 中创建 UMG Animation（Timeline 或 Color Blend）
- 通过绑定 C++ 中的 `ApplyUIStyle` 方法触发的 Blueprint Implementable Event 来启动颜色过渡动画
- 使用 `UMGSequencePlayer` 控制播放

**建议方案：** 在每个 Widget 的 C++ 中新增一个 BlueprintImplementableEvent：

```cpp
// 在 Widget 的 public 段声明
UFUNCTION(BlueprintImplementableEvent, Category = "UI Style")
void OnStyleApplied(const FUIStyleTableRow& Style);
```

在 `ApplyUIStyle` 方法末尾调用此事件，蓝图端可以响应并执行动画逻辑。

### 5.2 背景材质切换

- 通过 Dynamic Material Instance 的 Scalar/Vector 参数驱动背景颜色
- 在 Widget Blueprint 中创建材质实例，C++ 端通过 `UMaterialInstanceDynamic::SetScalarParameterValue` / `SetVectorParameterValue` 更新参数
- 适合需要特效背景（如发光、粒子叠加）的风格

### 5.3 字体加载

- `FontFamilyName` 字段存储字体资产路径
- 可通过 `FSoftObjectPath` 在运行时加载字体
- 也可在 `FUIStyleTableRow` 中新增 `FSlateFontInfo` 类型成员直接引用字体

## 6. 扩展指南

### 6.1 新增一个风格

只需在 DataTable `DT_UIStyles` 中添加一行新数据即可，无需修改任何代码。流程如下：

1. 打开 `Content/UI/DT_UIStyles`
2. 点击 "Add Row"
3. 填入新的 `StyleName` 和所有颜色/字体/布局参数
4. 保存即可
5. 如果 WorldSetupWidget 的 ComboBox 需要该选项，在 `WorldSetupWidget::NativeConstruct` 中添加对应的 `AddOption` 调用

### 6.2 支持 Custom 风格

当玩家在 WorldSetupWidget 中选择 "Custom" 时，可以：

- 读取玩家自定义的 JSON 配置文件
- 创建一个自定义的 `FUIStyleTableRow` 实例
- 通过 `UUIStyleSubsystem::SetActiveStyle` 扩展一个接受 `FUIStyleTableRow` 参数的重载方法
- 或者使用 `UDataAsset` 来存储自定义风格配置

```cpp
// 扩展方法（可选）
UFUNCTION(BlueprintCallable, Category = "UI Style")
void SetActiveStyleFromRow(const FUIStyleTableRow& CustomStyle);
```

### 6.3 运行时平滑过渡

使用 UMG Widget Animation 实现颜色渐变过渡：

1. 在 Widget Blueprint 中创建 Timeline 动画
2. Timeline 的关键帧控制颜色从旧值过渡到新值
3. 在 `OnStyleApplied` Blueprint Event 中触发 Timeline 播放
4. Timeline 的 Update 事件中驱动各 UI 元素的颜色属性

## 7. 代码修改清单

### 新建文件

| 文件路径 | 说明 |
|----------|------|
| `Source/PluginExample/Public/UIStyleTableRow.h` | 风格表行结构体定义 |
| `Source/PluginExample/Public/UIStyleSubsystem.h` | 风格管理子系统头文件 |
| `Source/PluginExample/Private/UIStyleSubsystem.cpp` | 风格管理子系统实现 |
| `Content/UI/DT_UIStyles.uasset` | 风格数据表（在编辑器中创建） |

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `Source/PluginExample/Public/DialogueWidget.h` | 新增 `ApplyUIStyle` 方法声明 + `CachedStyle` 成员 + `OnStyleApplied` BlueprintImplementableEvent |
| `Source/PluginExample/Private/DialogueWidget.cpp` | 实现 `ApplyUIStyle` 方法 + 修改 `AddMessageToHistory` 使用风格颜色 |
| `Source/PluginExample/Public/LoadingWidget.h` | 新增 `ApplyUIStyle` 方法声明 + `OnStyleApplied` BlueprintImplementableEvent |
| `Source/PluginExample/Private/LoadingWidget.cpp` | 实现 `ApplyUIStyle` 方法 |
| `Source/PluginExample/Public/WorldSetupWidget.h` | 新增 `ApplyUIStyle` 方法声明 + 可选 BindWidget 成员（TitleText/BackgroundBorder/SubtitleText）+ `OnStyleApplied` BlueprintImplementableEvent |
| `Source/PluginExample/Private/WorldSetupWidget.cpp` | 实现 `ApplyUIStyle` 方法 + 修改 `OnStartGameClicked` 调用 `SetActiveStyle` |
| `Source/PluginExample/Private/BasePlayerController.cpp` | 修改 `ShowLoadingScreen` 在创建 LoadingWidget 后调用 `ApplyStyleToLoading`；创建 DialogueWidget 后调用 `ApplyStyleToDialogue` |
| `Source/PluginExample/Private/PluginGameInstance.cpp` | 在 `Init` 中初始化风格子系统默认风格 |

### Build.cs 修改

无需修改。现有的 `PublicDependencyModuleNames` 已包含 `CoreUObject`、`Engine`、`UMG` 等所有必需模块。`UGameInstanceSubsystem` 和 `UDataTable` 均在 `Engine` 模块中。

## 附录：代码规范检查清单

| 检查项 | 状态 | 说明 |
|--------|------|------|
| FString 使用 `TEXT()` 宏 | 通过 | 所有字符串字面量均使用 `TEXT()` 包裹 |
| 委托绑定使用 `AddDynamic` | 通过 | 按钮点击等使用 `AddDynamic` |
| 多播委托使用 `Broadcast()` | 通过 | `OnWorldSetupComplete` 等使用 `Broadcast()` |
| UPROPERTY 类型支持反射 | 通过 | 所有 `UPROPERTY` 均使用 UE 支持的反射类型（`FString`、`FLinearColor`、`int32`、`float`、`TObjectPtr`） |
| 使用 `TObjectPtr` 而非裸指针 | 通过 | `StyleDataTable` 使用 `TObjectPtr<UDataTable>`；Widget 成员变量均使用 `TObjectPtr` |
| `FTableRowBase` 继承 | 通过 | `FUIStyleTableRow` 继承 `FTableRowBase` |
| 避免使用 `LR"()"` 原始字符串 | 通过 | 无原始字符串用法 |
