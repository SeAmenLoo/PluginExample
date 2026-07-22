# 数值化属性判定系统 - 功能开发文档

## 1. 功能概述

本系统在项目现有 AI 结构化文生文框架（LLMRuntime + ChunkManagerComponent）之上，构建一套完整的数值化属性判定机制，涵盖三个核心子系统：

- **属性生成系统**：在世界观生成阶段，通过 LLM 为主角生成符合世界观设定的结构化属性集（战斗力、敏捷、法力、口才等）
- **属性判定与分支系统**：在对话过程中，根据对话内容或选项触发属性判定事件，支持多属性并集（OR）/交集（AND）判定，并根据结果触发不同对话分支
- **骰子判定子系统**：参考《极乐迪斯科》与《博德之门3》，为属性判定引入随机性，支持多种骰子类型、优势/劣势、暴击成功/失败

系统通过扩展现有的 JSON 结构化响应格式实现 LLM 与游戏逻辑的协同：LLM 负责叙事上下文判断（何时需要判定、判定什么属性、难度如何），游戏引擎负责数值计算（骰子投掷、属性比较、分支选择）。

## 2. 可行性分析

### 2.1 现有框架支持点

项目的 AI 结构化文生文框架已经具备以下能力，本系统可直接复用：

| 现有能力 | 位置 | 复用方式 |
|---------|------|---------|
| JSON 结构化 LLM 响应 | `ULLMChatCompletionAsyncAction` + `bForceJsonObject` | 扩展响应 JSON Schema |
| 世界观生成提示词 | `BuildWorldGenerationSystemPrompt()` | 新增 `player_attributes` 字段 |
| 对话响应解析 | `HandleDialogueResponse()` | 新增属性判定/修改字段解析 |
| 对话动作系统 | `FDialogueAction` + `EDialogueActionType` | 新增判定/修改动作类型 |
| 对话上下文构建 | `BuildDialogueContextInfo()` | 注入当前属性状态 |
| 异步 LLM 调用 | `ULLMChatCompletionAsyncAction::ChatCompletion` | 判定分支的二次请求 |
| JSON Schema 约束 | `FLLMChatRequest::JsonSchema` | 约束属性生成格式 |

### 2.2 技术可行性结论

**整体可行，无重大技术阻碍。** 关键判断如下：

1. **属性生成**：世界生成 JSON 已包含 `player_appearance` 等结构化字段，新增 `player_attributes` 数组字段不破坏现有解析逻辑（`ParseInitialWorldResponse` 使用 `HasField` 守卫，未知字段自动忽略）

2. **属性判定**：对话响应 JSON 已支持 `actions` 和 `choice_actions`，新增 `attribute_checks` 字段与之并列，解析模式完全一致（`TSharedRef<TJsonReader<>>` + `FJsonSerializer::Deserialize`）

3. **骰子系统**：纯本地随机数计算，不依赖 LLM，使用 `FRandomStream` 保证可种子化复现，无网络依赖

4. **分支触发**：LLM 在同一响应中提供 success/failure 两套文本和动作，引擎根据骰子结果选择展示哪一套，避免二次 LLM 调用的延迟

5. **属性修改**：与现有 `ExecuteDialogueActions` 模式一致，新增 `modify_attribute` 动作类型即可

### 2.3 主要风险

| 风险 | 等级 | 缓解方案 |
|------|------|---------|
| LLM 生成的属性不符合世界观 | 中 | 提示词中给出属性范式约束 + JSON Schema 校验 + 引擎端默认值兜底 |
| LLM 漏输出判定字段 | 中 | 服务器端对 `attribute_checks` 做可选处理，缺失时按普通对话继续 |
| 骰子结果导致剧情卡死 | 低 | 失败分支必须提供后续路径（退出对话/重试/替代方案），提示词强制要求 |
| 属性数值膨胀 | 低 | 设置 MinValue/MaxValue 钳制，单次修改幅度限制 |
| Token 消耗增加 | 低 | 属性状态以紧凑格式注入上下文，避免完整属性表重复传输 |

## 3. 系统架构设计

### 3.1 模块划分

系统新增一个独立的 GameInstanceSubsystem `UAttributeSystemSubsystem`，负责属性存储、判定、骰子逻辑，与 `UChunkManagerComponent` 解耦协作：

```
UPluginGameInstance
├── UChunkManagerComponent (现有)
│   ├── 世界生成 / 分块管理 / 对话流程
│   └── 调用 UAttributeSystemSubsystem 处理属性相关逻辑
└── UAttributeSystemSubsystem (新增)
    ├── 属性集存储与管理
    ├── 属性判定（AND/OR 逻辑）
    ├── 骰子投掷（D20/D6 等多种类型）
    └── 属性修改与钳制
```

### 3.2 数据流

```
玩家输入世界观提示词
        ↓
ChunkManagerComponent::GenerateWorldFromPrompt()
        ↓
LLM 返回世界配置 JSON（含 player_attributes）
        ↓
ParseInitialWorldResponse() 解析属性 → UAttributeSystemSubsystem::InitializeAttributes()
        ↓
=== 对话阶段 ===
玩家选择对话选项 / 输入文本
        ↓
ChunkManagerComponent::SendDialogueRequest()
├── BuildDialogueContextInfo() 注入当前属性状态
└── BuildDialogueSystemPrompt() 包含属性判定规则
        ↓
LLM 返回对话响应 JSON（含 attribute_checks / attribute_modifications）
        ↓
HandleDialogueResponse() 解析
├── 解析 attribute_checks → UAttributeSystemSubsystem::ExecuteAttributeCheck()
│   ├── 执行骰子投掷
│   ├── 计算属性修正值
│   ├── 判定成功/失败
│   └── 返回结果（含分支文本、动作、属性修改）
├── 解析 attribute_modifications → UAttributeSystemSubsystem::ModifyAttribute()
└── 根据判定结果选择展示 success_text 或 failure_text
        ↓
DialogueWidget 显示判定结果 + 对应分支文本
```

## 4. 数据结构设计

### 4.1 属性定义结构体

在 `GameDataTypes.h` 中新增以下结构体（遵循项目约定：英文枚举标识符 + 中文 DisplayName）：

```cpp
/** 属性类型枚举 */
UENUM(BlueprintType)
enum class EAttributeType : uint8
{
    Physical    UMETA(DisplayName = "物理"),
    Mental      UMETA(DisplayName = "精神"),
    Social      UMETA(DisplayName = "社交"),
    Special     UMETA(DisplayName = "特殊")
};

/** 单个属性定义 */
USTRUCT(BlueprintType)
struct FPlayerAttribute
{
    GENERATED_BODY()

    /** 属性唯一标识，如 "combat"、"agility"、"magic"、"eloquence" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    FString AttributeId;

    /** 显示名称，如 "战斗力"、"敏捷"、"法力"、"口才" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    FString DisplayName;

    /** 属性分类 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    EAttributeType AttributeType = EAttributeType::Physical;

    /** 当前值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    int32 CurrentValue = 10;

    /** 最小值（默认 1） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    int32 MinValue = 1;

    /** 最大值（默认 20） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    int32 MaxValue = 20;

    /** 属性描述（供 LLM 理解属性含义） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    FString Description;
};
```

### 4.2 判定逻辑结构体

```cpp
/** 判定逻辑类型 */
UENUM(BlueprintType)
enum class ECheckLogic : uint8
{
    Intersection    UMETA(DisplayName = "交集（AND）"),
    Union           UMETA(DisplayName = "并集（OR）")
};

/** 骰子类型 */
UENUM(BlueprintType)
enum class EDiceType : uint8
{
    D4     UMETA(DisplayName = "D4"),
    D6     UMETA(DisplayName = "D6"),
    D8     UMETA(DisplayName = "D8"),
    D10    UMETA(DisplayName = "D10"),
    D12    UMETA(DisplayName = "D12"),
    D20    UMETA(DisplayName = "D20"),
    D100   UMETA(DisplayName = "D100")
};

/** 骰子优势状态 */
UENUM(BlueprintType)
enum class EDiceAdvantage : uint8
{
    Normal        UMETA(DisplayName = "正常"),
    Advantage     UMETA(DisplayName = "优势（取较高）"),
    Disadvantage  UMETA(DisplayName = "劣势（取较低）")
};

/** 骰子投掷结果 */
USTRUCT(BlueprintType)
struct FDiceRollResult
{
    GENERATED_BODY()

    /** 骰子类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    EDiceType DiceType = EDiceType::D20;

    /** 骰子数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    int32 DiceCount = 1;

    /** 每个骰子的原始点数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    TArray<int32> RawRolls;

    /** 优势/劣势时的投掷记录（用于 UI 展示） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    TArray<int32> DiscardedRolls;

    /** 骰子原始点数总和（取较高/较低后的值） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    int32 DiceTotal = 0;

    /** 属性修正值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    int32 AttributeModifier = 0;

    /** 最终判定值 = DiceTotal + AttributeModifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    int32 FinalValue = 0;

    /** 难度等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    int32 DifficultyClass = 10;

    /** 是否暴击成功（原始点数为骰子最大值） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    bool bCriticalSuccess = false;

    /** 是否暴击失败（D20 原始点数为 1） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    bool bCriticalFailure = false;

    /** 是否判定成功 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Dice")
    bool bSuccess = false;
};

/** 属性修改项 */
USTRUCT(BlueprintType)
struct FAttributeModification
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    FString AttributeId;

    /** 增减量，正数增加负数减少 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    int32 Delta = 0;

    /** 修改原因（供 UI 和日志展示） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    FString Reason;
};

/** 属性判定请求（从 LLM 响应解析） */
USTRUCT(BlueprintType)
struct FAttributeCheckRequest
{
    GENERATED_BODY()

    /** 判定唯一标识 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    FString CheckId;

    /** 判定名称，如 "说服守卫"、"闪避陷阱" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    FString CheckName;

    /** 参与判定的属性 ID 列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    TArray<FString> AttributeIds;

    /** 判定逻辑：交集=所有属性都要达标，并集=任一属性达标即可 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    ECheckLogic Logic = ECheckLogic::Intersection;

    /** 难度等级（骰子结果+修正值需 >= 此值才算成功） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    int32 DifficultyClass = 10;

    /** 骰子类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    EDiceType DiceType = EDiceType::D20;

    /** 骰子数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    int32 DiceCount = 1;

    /** 优势/劣势 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    EDiceAdvantage Advantage = EDiceAdvantage::Normal;

    /** 成功时展示的文本 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    FString SuccessText;

    /** 失败时展示的文本 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    FString FailureText;

    /** 成功时执行的动作 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    TArray<FDialogueAction> SuccessActions;

    /** 失败时执行的动作 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    TArray<FDialogueAction> FailureActions;

    /** 成功时的属性修改 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    TArray<FAttributeModification> SuccessModifications;

    /** 失败时的属性修改 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    TArray<FAttributeModification> FailureModifications;
};

/** 属性判定完整结果 */
USTRUCT(BlueprintType)
struct FAttributeCheckResult
{
    GENERATED_BODY()

    /** 原始判定请求 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    FAttributeCheckRequest Request;

    /** 骰子投掷结果 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    FDiceRollResult DiceResult;

    /** 实际展示的文本（成功或失败） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    FString DisplayedText;

    /** 实际执行的动作（成功或失败） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    TArray<FDialogueAction> ExecutedActions;

    /** 实际执行的属性修改 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Check")
    TArray<FAttributeModification> ExecutedModifications;
};
```

### 4.3 动作类型扩展

在 `GameDataTypes.h` 的 `EDialogueActionType` 枚举中新增两个值：

```cpp
enum class EDialogueActionType : uint8
{
    None            UMETA(DisplayName = "None"),
    EndDialogue     UMETA(DisplayName = "End Dialogue"),
    UnlockExit      UMETA(DisplayName = "Unlock Exit"),
    GiveItem        UMETA(DisplayName = "Give Item"),
    StartQuest      UMETA(DisplayName = "Start Quest"),
    ChangeNPCFaction UMETA(DisplayName = "Change NPC Faction"),
    TriggerCheck    UMETA(DisplayName = "Trigger Attribute Check"),  // 新增
    ModifyAttribute UMETA(DisplayName = "Modify Attribute"),          // 新增
    Custom          UMETA(DisplayName = "Custom")
};
```

`FDialogueAction` 结构体新增字段：

```cpp
struct FDialogueAction
{
    // ... 现有字段 ...

    /** 触发属性判定时使用的判定 ID（配合 TriggerCheck 类型） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CheckId;

    /** 直接修改属性时的属性 ID（配合 ModifyAttribute 类型） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AttributeId;

    /** 属性增减量（配合 ModifyAttribute 类型） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AttributeDelta = 0;
};
```

### 4.4 世界配置扩展

在 `FWorldConfig` 中新增属性集字段：

```cpp
struct FWorldConfig
{
    // ... 现有字段 ...

    /** LLM 生成的玩家初始属性集 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FPlayerAttribute> PlayerAttributes;
};
```

## 5. 属性生成系统

### 5.1 子系统实现

新增 `UAttributeSystemSubsystem`（GameInstanceSubsystem），负责属性的全生命周期管理：

```cpp
UCLASS()
class PLUGINEXAMPLE_API UAttributeSystemSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // ==================== 属性管理 ====================

    /** 用 LLM 生成的属性列表初始化玩家属性 */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    void InitializeAttributes(const TArray<FPlayerAttribute>& Attributes);

    /** 获取所有属性 */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    TArray<FPlayerAttribute> GetAllAttributes() const;

    /** 按 ID 获取属性 */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    bool TryGetAttribute(const FString& AttributeId, FPlayerAttribute& OutAttribute) const;

    /** 修改属性值（自动钳制到 Min/Max 范围） */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    int32 ModifyAttribute(const FString& AttributeId, int32 Delta);

    /** 直接设置属性值（自动钳制） */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    void SetAttributeValue(const FString& AttributeId, int32 NewValue);

    /** 计算属性修正值（D&D 风格：(value - 10) / 2，向下取整） */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    int32 GetAttributeModifier(const FString& AttributeId) const;

    // ==================== 属性判定 ====================

    /** 执行一次完整的属性判定（含骰子投掷） */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    FAttributeCheckResult ExecuteAttributeCheck(const FAttributeCheckRequest& Request);

    // ==================== 骰子系统 ====================

    /** 执行骰子投掷 */
    UFUNCTION(BlueprintCallable, Category = "RPG|Dice")
    FDiceRollResult RollDice(EDiceType DiceType, int32 Count, int32 AttributeModifier,
                             EDiceAdvantage Advantage, int32 DifficultyClass);

    // ==================== 上下文与序列化 ====================

    /** 生成紧凑的属性状态文本（供 LLM 上下文注入） */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    FString BuildAttributeContextString() const;

    /** 导出属性到 JSON */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    FString ExportToJson() const;

    /** 从 JSON 导入属性 */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    bool ImportFromJson(const FString& JsonString);

    // ==================== 事件 ====================

    /** 属性值变化事件 */
    UPROPERTY(BlueprintAssignable)
    FOnAttributeChanged OnAttributeChanged;

    /** 属性判定完成事件 */
    UPROPERTY(BlueprintAssignable)
    FOnAttributeCheckCompleted OnAttributeCheckCompleted;

private:
    /** 属性存储（按 AttributeId 索引） */
    UPROPERTY()
    TMap<FString, FPlayerAttribute> AttributeMap;

    /** 骰子随机流（可种子化） */
    FRandomStream DiceRandomStream;

    /** 获取骰子最大面数 */
    int32 GetDiceMaxValue(EDiceType DiceType) const;
};
```

委托声明：

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAttributeChanged,
    const FString&, AttributeId, int32, OldValue, int32, NewValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeCheckCompleted,
    const FAttributeCheckResult&, Result);
```

### 5.2 LLM 属性生成提示词

在 `BuildWorldGenerationSystemPrompt()` 中扩展 JSON 结构，新增 `player_attributes` 字段。扩展后的提示词在现有 JSON 结构中增加：

```
"player_attributes": [
    {
        "id": "combat",
        "display_name": "战斗力",
        "type": "physical",
        "value": 12,
        "min": 1,
        "max": 20,
        "description": "角色的物理战斗能力，影响战斗判定"
    },
    {
        "id": "agility",
        "display_name": "敏捷",
        "type": "physical",
        "value": 14,
        "min": 1,
        "max": 20,
        "description": "角色的反应速度和闪避能力"
    }
]
```

提示词补充约束段落：

```
属性生成规则：
- 根据世界观生成 4-8 个符合设定的主角属性
- 属性类型仅限: physical（物理）、mental（精神）、social（社交）、special（特殊）
- 属性值范围 1-20，初始值总和建议在 60-80 之间
- 中世纪奇幻世界典型属性: 战斗力、敏捷、法力、口才、感知、体质
- 科幻世界典型属性: 科技、黑客、魅力、反应、智力、体能
- 侦探世界典型属性: 逻辑、共情、智力、说服、直觉、意志
- 每个属性必须有清晰的 description，说明其在判定中的作用
```

### 5.3 属性解析

在 `ParseInitialWorldResponse()` 中新增属性解析逻辑：

```cpp
if (JsonObject->HasField(TEXT("player_attributes")))
{
    const TArray<TSharedPtr<FJsonValue>>* AttrArray;
    if (JsonObject->TryGetArrayField(TEXT("player_attributes"), AttrArray))
    {
        TArray<FPlayerAttribute> ParsedAttributes;
        for (const auto& AttrVal : *AttrArray)
        {
            const TSharedPtr<FJsonObject>& AttrObj = AttrVal->AsObject();
            if (!AttrObj.IsValid()) continue;

            FPlayerAttribute Attr;
            Attr.AttributeId = AttrObj->GetStringField(TEXT("id"));
            Attr.DisplayName = AttrObj->GetStringField(TEXT("display_name"));
            Attr.CurrentValue = AttrObj->GetNumberField(TEXT("value"));
            Attr.MinValue = AttrObj->HasField(TEXT("min")) ?
                AttrObj->GetNumberField(TEXT("min")) : 1;
            Attr.MaxValue = AttrObj->HasField(TEXT("max")) ?
                AttrObj->GetNumberField(TEXT("max")) : 20;
            Attr.Description = AttrObj->GetStringField(TEXT("description"));

            // 解析 type 字符串到枚举
            FString TypeStr = AttrObj->GetStringField(TEXT("type"));
            if (TypeStr == TEXT("physical")) Attr.AttributeType = EAttributeType::Physical;
            else if (TypeStr == TEXT("mental")) Attr.AttributeType = EAttributeType::Mental;
            else if (TypeStr == TEXT("social")) Attr.AttributeType = EAttributeType::Social;
            else Attr.AttributeType = EAttributeType::Special;

            ParsedAttributes.Add(Attr);
        }

        WorldConfig.PlayerAttributes = ParsedAttributes;

        // 初始化属性子系统
        if (UAttributeSystemSubsystem* AttrSys = GetGameInstance()
            ->GetSubsystem<UAttributeSystemSubsystem>())
        {
            AttrSys->InitializeAttributes(ParsedAttributes);
        }
    }
}
```

## 6. 属性判定系统

### 6.1 判定逻辑

判定系统支持两种逻辑模式：

**交集（AND）**：所有参与属性都必须单独通过骰子判定，整体才算成功。适用于需要多方面能力的复杂任务（如"潜行通过"需要敏捷+感知同时达标）。

**并集（OR）**：任一属性通过骰子判定即算成功。适用于可以用不同方式解决的任务（如"打开门"可以用力量撞开或用技巧撬开）。

### 6.2 单属性判定流程

对于每个参与判定的属性，执行以下步骤：

1. 计算属性修正值：`Modifier = (AttributeValue - 10) / 2`（D&D 5e 风格，向下取整）
2. 执行骰子投掷（见第 7 节）
3. 计算最终值：`FinalValue = DiceTotal + Modifier`
4. 暴击判定：
   - D20 暴击成功：原始点数为 20，无视难度直接成功
   - D20 暴击失败：原始点数为 1，无视难度直接失败
5. 普通判定：`bSuccess = (FinalValue >= DifficultyClass)`

### 6.3 多属性组合判定

```cpp
FAttributeCheckResult UAttributeSystemSubsystem::ExecuteAttributeCheck(
    const FAttributeCheckRequest& Request)
{
    FAttributeCheckResult Result;
    Result.Request = Request;

    bool bOverallSuccess = false;

    if (Request.AttributeIds.Num() == 0)
    {
        // 无属性参与，纯骰子判定
        FDiceRollResult DiceResult = RollDice(Request.DiceType, Request.DiceCount,
            0, Request.Advantage, Request.DifficultyClass);
        bOverallSuccess = DiceResult.bSuccess;
        Result.DiceResult = DiceResult;
    }
    else if (Request.Logic == ECheckLogic::Intersection)
    {
        // AND 逻辑：所有属性都必须成功
        bOverallSuccess = true;
        for (const FString& AttrId : Request.AttributeIds)
        {
            int32 Modifier = GetAttributeModifier(AttrId);
            FDiceRollResult DiceResult = RollDice(Request.DiceType, Request.DiceCount,
                Modifier, Request.Advantage, Request.DifficultyClass);
            // 取第一个属性的骰子结果用于展示（或可扩展为多骰子结果数组）
            if (Result.DiceResult.FinalValue == 0)
            {
                Result.DiceResult = DiceResult;
            }
            if (!DiceResult.bSuccess)
            {
                bOverallSuccess = false;
                break;  // 短路评估
            }
        }
    }
    else
    {
        // OR 逻辑：任一属性成功即可
        bOverallSuccess = false;
        for (const FString& AttrId : Request.AttributeIds)
        {
            int32 Modifier = GetAttributeModifier(AttrId);
            FDiceRollResult DiceResult = RollDice(Request.DiceType, Request.DiceCount,
                Modifier, Request.Advantage, Request.DifficultyClass);
            if (Result.DiceResult.FinalValue == 0)
            {
                Result.DiceResult = DiceResult;
            }
            if (DiceResult.bSuccess)
            {
                bOverallSuccess = true;
                Result.DiceResult = DiceResult;  // 记录成功的骰子结果
                break;  // 短路评估
            }
        }
    }

    // 根据成功/失败填充展示文本、动作、修改
    if (bOverallSuccess)
    {
        Result.DisplayedText = Request.SuccessText;
        Result.ExecutedActions = Request.SuccessActions;
        Result.ExecutedModifications = Request.SuccessModifications;
    }
    else
    {
        Result.DisplayedText = Request.FailureText;
        Result.ExecutedActions = Request.FailureActions;
        Result.ExecutedModifications = Request.FailureModifications;
    }

    Result.DiceResult.bSuccess = bOverallSuccess;

    // 执行属性修改
    for (const FAttributeModification& Mod : Result.ExecutedModifications)
    {
        ModifyAttribute(Mod.AttributeId, Mod.Delta);
    }

    // 广播事件
    OnAttributeCheckCompleted.Broadcast(Result);

    return Result;
}
```

### 6.4 判定结果注入对话

在 `HandleDialogueResponse()` 中，解析到 `attribute_checks` 后：

1. 调用 `UAttributeSystemSubsystem::ExecuteAttributeCheck()` 执行判定
2. 用判定结果的 `DisplayedText` 替换或追加到 `response_text` 展示
3. 执行 `ExecutedActions`（与现有 `ExecuteDialogueActions` 复用）
4. 通过 `OnAttributeCheckCompleted` 委托通知 UI 展示骰子动画

## 7. 骰子判定系统

### 7.1 骰子投掷实现

```cpp
FDiceRollResult UAttributeSystemSubsystem::RollDice(
    EDiceType DiceType, int32 Count, int32 AttributeModifier,
    EDiceAdvantage Advantage, int32 DifficultyClass)
{
    FDiceRollResult Result;
    Result.DiceType = DiceType;
    Result.DiceCount = Count;
    Result.AttributeModifier = AttributeModifier;
    Result.DifficultyClass = DifficultyClass;

    int32 MaxValue = GetDiceMaxValue(DiceType);

    // 优势/劣势时投掷两倍骰子
    int32 RollTimes = (Advantage != EDiceAdvantage::Normal) ? Count * 2 : Count;

    TArray<int32> AllRolls;
    for (int32 i = 0; i < RollTimes; ++i)
    {
        AllRolls.Add(DiceRandomStream.RandRange(1, MaxValue));
    }

    if (Advantage == EDiceAdvantage::Normal)
    {
        Result.RawRolls = AllRolls;
        for (int32 Roll : AllRolls)
        {
            Result.DiceTotal += Roll;
        }
    }
    else
    {
        // 优势/劣势：每次取较高/较低
        for (int32 i = 0; i < Count; ++i)
        {
            int32 Roll1 = AllRolls[i * 2];
            int32 Roll2 = AllRolls[i * 2 + 1];

            int32 Kept, Discarded;
            if (Advantage == EDiceAdvantage::Advantage)
            {
                Kept = FMath::Max(Roll1, Roll2);
                Discarded = FMath::Min(Roll1, Roll2);
            }
            else
            {
                Kept = FMath::Min(Roll1, Roll2);
                Discarded = FMath::Max(Roll1, Roll2);
            }
            Result.RawRolls.Add(Kept);
            Result.DiscardedRolls.Add(Discarded);
            Result.DiceTotal += Kept;
        }
    }

    Result.FinalValue = Result.DiceTotal + AttributeModifier;

    // 暴击判定（仅 D20）
    if (DiceType == EDiceType::D20 && Count == 1)
    {
        int32 NaturalRoll = Result.RawRolls[0];
        if (NaturalRoll == 20)
        {
            Result.bCriticalSuccess = true;
            Result.bSuccess = true;
            return Result;
        }
        if (NaturalRoll == 1)
        {
            Result.bCriticalFailure = true;
            Result.bSuccess = false;
            return Result;
        }
    }

    Result.bSuccess = (Result.FinalValue >= DifficultyClass);
    return Result;
}

int32 UAttributeSystemSubsystem::GetDiceMaxValue(EDiceType DiceType) const
{
    switch (DiceType)
    {
        case EDiceType::D4:   return 4;
        case EDiceType::D6:   return 6;
        case EDiceType::D8:   return 8;
        case EDiceType::D10:  return 10;
        case EDiceType::D12:  return 12;
        case EDiceType::D20:  return 20;
        case EDiceType::D100: return 100;
        default:              return 20;
    }
}
```

### 7.2 骰子模式预设

系统支持两种预设模式，可在 `UAttributeSystemSubsystem` 中通过配置切换：

| 模式 | 骰子 | 修正值公式 | 暴击 | 适用场景 |
|------|------|-----------|------|---------|
| BG3 模式 | 1D20 | (属性值 - 10) / 2 | 自然20成功/自然1失败 | 属性值范围 1-20 |
| 极乐迪斯科模式 | 2D6 | 属性值直接加 | 无暴击 | 属性值范围 1-15 |

默认采用 BG3 模式（D20），与属性值 1-20 的范围匹配。

### 7.3 随机种子

`DiceRandomStream` 支持种子化，便于：
- 存档/读档时复现判定结果
- 调试时固定随机序列
- 测试时验证判定逻辑

```cpp
// 在 Initialize 中根据世界 ID 生成种子
void UAttributeSystemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // 默认使用时间种子，世界生成后可重设
    DiceRandomStream.Initialize(FDateTime::Now().GetTicks());
}

// 世界生成完成后，用世界 ID 固定种子
void UAttributeSystemSubsystem::SetSeedFromWorldId(const FString& WorldId)
{
    uint32 Seed = FCrc::StrCrc32(*WorldId);
    DiceRandomStream.Initialize(Seed);
}
```

## 8. 对话分支系统

### 8.1 LLM 对话响应 JSON 扩展

在 `BuildDialogueSystemPrompt()` 中扩展 JSON 格式，新增三个字段：

```json
{
  "response_text": "NPC 的常规回复",
  "npc_expression": "neutral",
  "player_expression": "neutral",
  "choices": ["选项1", "选项2", "选项3"],
  "actions": [{"type": "modify_attribute", "attribute_id": "combat", "delta": 1}],
  "choice_actions": {
    "0": {"type": "trigger_check", "check_id": "persuade_1"}
  },
  "attribute_checks": [
    {
      "check_id": "persuade_1",
      "check_name": "说服守卫",
      "attributes": ["eloquence", "charisma"],
      "logic": "union",
      "difficulty": 12,
      "dice_type": "d20",
      "dice_count": 1,
      "advantage": "normal",
      "success_text": "守卫被你的言辞打动，缓缓侧身让开了道路。",
      "failure_text": "守卫冷哼一声，丝毫不为所动。你不得不另想办法。",
      "success_actions": [{"type": "unlock_exit", "exit_id": "exit_north"}],
      "failure_actions": [],
      "success_modifications": [{"attribute_id": "eloquence", "delta": 1}],
      "failure_modifications": []
    }
  ],
  "attribute_modifications": [
    {"attribute_id": "combat", "delta": 1, "reason": "经历了一场战斗训练"}
  ]
}
```

### 8.2 触发时机

属性判定有两种触发方式：

**立即判定**：LLM 在 `actions` 中直接包含完整判定（`attribute_checks` 中定义，`actions` 中引用）。适用于 NPC 主动发起的判定（如 NPC 质疑玩家身份时触发感知判定）。

**选项触发**：LLM 在 `choice_actions` 中为特定选项标记 `trigger_check`，玩家选择该选项时触发对应判定。适用于玩家主动选择的行为（如选择"试图说服守卫"选项时触发口才判定）。

### 8.3 对话响应解析扩展

在 `HandleDialogueResponse()` 中新增解析逻辑：

```cpp
// 解析属性判定请求
TMap<FString, FAttributeCheckRequest> CheckRequests;
if (JsonObject->HasField(TEXT("attribute_checks")))
{
    const TArray<TSharedPtr<FJsonValue>>* CheckArray;
    if (JsonObject->TryGetArrayField(TEXT("attribute_checks"), CheckArray))
    {
        for (const auto& CheckVal : *CheckArray)
        {
            const TSharedPtr<FJsonObject>& CheckObj = CheckVal->AsObject();
            if (!CheckObj.IsValid()) continue;

            FAttributeCheckRequest CheckReq;
            CheckReq.CheckId = CheckObj->GetStringField(TEXT("check_id"));
            CheckReq.CheckName = CheckObj->GetStringField(TEXT("check_name"));
            CheckReq.DifficultyClass = CheckObj->GetNumberField(TEXT("difficulty"));

            // 解析属性列表
            const TArray<TSharedPtr<FJsonValue>>* AttrIds;
            if (CheckObj->TryGetArrayField(TEXT("attributes"), AttrIds))
            {
                for (const auto& AttrId : *AttrIds)
                {
                    CheckReq.AttributeIds.Add(AttrId->AsString());
                }
            }

            // 解析逻辑类型
            FString LogicStr = CheckObj->GetStringField(TEXT("logic"));
            CheckReq.Logic = (LogicStr == TEXT("union")) ?
                ECheckLogic::Union : ECheckLogic::Intersection;

            // 解析骰子配置
            FString DiceStr = CheckObj->GetStringField(TEXT("dice_type"));
            CheckReq.DiceType = ParseDiceType(DiceStr);
            CheckReq.DiceCount = CheckObj->HasField(TEXT("dice_count")) ?
                CheckObj->GetNumberField(TEXT("dice_count")) : 1;

            // 解析优势
            if (CheckObj->HasField(TEXT("advantage")))
            {
                FString AdvStr = CheckObj->GetStringField(TEXT("advantage"));
                if (AdvStr == TEXT("advantage"))
                    CheckReq.Advantage = EDiceAdvantage::Advantage;
                else if (AdvStr == TEXT("disadvantage"))
                    CheckReq.Advantage = EDiceAdvantage::Disadvantage;
            }

            // 解析成功/失败分支
            CheckReq.SuccessText = CheckObj->GetStringField(TEXT("success_text"));
            CheckReq.FailureText = CheckObj->GetStringField(TEXT("failure_text"));

            // 解析分支动作和属性修改（略，模式与 actions 解析一致）
            CheckRequests.Add(CheckReq.CheckId, CheckReq);
        }
    }
}

// 解析直接属性修改
TArray<FAttributeModification> DirectModifications;
if (JsonObject->HasField(TEXT("attribute_modifications")))
{
    const TArray<TSharedPtr<FJsonValue>>* ModArray;
    if (JsonObject->TryGetArrayField(TEXT("attribute_modifications"), ModArray))
    {
        for (const auto& ModVal : *ModArray)
        {
            const TSharedPtr<FJsonObject>& ModObj = ModVal->AsObject();
            FAttributeModification Mod;
            Mod.AttributeId = ModObj->GetStringField(TEXT("attribute_id"));
            Mod.Delta = ModObj->GetNumberField(TEXT("delta"));
            if (ModObj->HasField(TEXT("reason")))
                Mod.Reason = ModObj->GetStringField(TEXT("reason"));
            DirectModifications.Add(Mod);
        }
    }
}
```

### 8.4 判定执行与分支展示

当解析到 `actions` 中有 `trigger_check` 类型，或 `choice_actions` 中某选项触发 `trigger_check` 时：

```cpp
void UChunkManagerComponent::ExecuteAttributeCheck(const FAttributeCheckRequest& CheckReq)
{
    UAttributeSystemSubsystem* AttrSys = GetGameInstance()
        ->GetSubsystem<UAttributeSystemSubsystem>();
    if (!AttrSys) return;

    // 执行判定
    FAttributeCheckResult Result = AttrSys->ExecuteAttributeCheck(CheckReq);

    // 在对话界面展示判定结果文本
    if (ActiveDialogueWidget)
    {
        // 先展示判定信息（骰子点数、属性修正、难度、结果）
        FString CheckInfo = FString::Printf(
            TEXT("【%s】 骰子: %d + 修正: %d = %d vs DC %d → %s"),
            *CheckReq.CheckName,
            Result.DiceResult.DiceTotal,
            Result.DiceResult.AttributeModifier,
            Result.DiceResult.FinalValue,
            Result.DiceResult.DifficultyClass,
            Result.DiceResult.bSuccess ? TEXT("成功") : TEXT("失败")
        );
        ActiveDialogueWidget->AddMessageToHistory(TEXT("系统"), CheckInfo, false);

        // 展示判定后的分支文本
        ActiveDialogueWidget->ShowNPCResponse(
            CurrentNPCData.Name, Result.DisplayedText, TArray<FString>());
    }

    // 执行判定结果关联的动作
    if (Result.ExecutedActions.Num() > 0)
    {
        ExecuteDialogueActions(Result.ExecutedActions);
    }
}
```

### 8.5 属性状态注入上下文

在 `BuildDialogueContextInfo()` 中追加当前属性状态：

```cpp
// 在现有 ContextInfo 构建中追加属性信息
if (UAttributeSystemSubsystem* AttrSys = GetGameInstance()
    ->GetSubsystem<UAttributeSystemSubsystem>())
{
    ContextInfo += TEXT("\n=== PLAYER ATTRIBUTES ===\n");
    ContextInfo += AttrSys->BuildAttributeContextString();
    ContextInfo += TEXT("\n=== END ATTRIBUTES ===");
}
```

`BuildAttributeContextString()` 生成紧凑格式：

```
战斗力(combat): 12 [修正+1] | 敏捷(agility): 14 [修正+2] | 法力(magic): 8 [修正-1] | 口才(eloquence): 15 [修正+2]
```

## 9. LLM 提示词设计

### 9.1 对话提示词扩展

在 `BuildDialogueSystemPrompt()` 中追加属性判定规则段落：

```
属性判定规则：
当对话中出现需要能力验证的情境时（战斗、闪避、说服、破解等），可以使用属性判定：
- 在 attribute_checks 数组中定义判定请求
- 在 actions 或 choice_actions 中通过 {"type": "trigger_check", "check_id": "xxx"} 触发判定
- 必须同时提供 success_text 和 failure_text 两套分支
- logic: "intersection" 表示所有属性都要达标，"union" 表示任一达标即可
- difficulty 建议 5-20 之间（简单 5-8，普通 10-12，困难 15-18，极难 20）
- dice_type 默认 "d20"，dice_count 默认 1
- advantage: "advantage"（优势，取较高）、"disadvantage"（劣势，取较低）、"normal"（正常）
- 失败分支必须提供后续路径，不能导致剧情卡死

属性修改规则：
- 在 attribute_modifications 中定义直接修改（不涉及判定）
- delta 为正数表示增加，负数表示减少
- 单次修改幅度建议 1-3，避免数值膨胀
- 修改应有合理的 reason（如"经过训练"、"受伤"、"获得灵感"）

使用时机建议：
- 战斗场景：战斗力/敏捷判定（闪避、命中、格挡）
- 交涉场景：口才/魅力判定（说服、欺骗、威吓）
- 探索场景：感知/智力判定（发现隐藏、解读文字、破解机关）
- 特殊场景：法力判定（施法、抵抗魔法）、体质判定（抵抗毒素、长时间奔跑）
```

### 9.2 属性生成提示词约束

在世界生成提示词中，属性生成的额外约束：

```
属性设计要求：
- 属性必须与世界观紧密关联，反映该世界的核心能力体系
- 属性 ID 使用英文小写+下划线（如 combat_power、magic_affinity）
- display_name 使用与世界观一致的语言
- 初始值总和控制在 60-80 之间（4-8 个属性，每个 8-15）
- 属性应覆盖物理、精神、社交三个维度，确保判定场景多样
- 特殊世界可增加 special 类型属性（如赛博朋克的"义体兼容性"）
```

## 10. UI 集成方案

### 10.1 属性面板 Widget

新增 `UAttributePanelWidget`，用于展示当前属性状态：

```
+----------------------------------+
|  角色属性                         |
|  --------------------------------|
|  [物理]                          |
|  战斗力    ████████░░ 12  (+1)   |
|  敏捷      █████████░ 14  (+2)   |
|  体质      ██████░░░░ 10  (+0)   |
|                                  |
|  [精神]                          |
|  法力      ████░░░░░░  8  (-1)   |
|  感知      █████████░ 14  (+2)   |
|                                  |
|  [社交]                          |
|  口才      ██████████ 15  (+2)   |
|  魅力      ███████░░░ 11  (+0)   |
+----------------------------------+
```

可通过快捷键（如 Tab）在对话中唤出属性面板。

### 10.2 骰子判定 UI

在 `UDialogueWidget` 中新增骰子判定展示区域，判定触发时：

1. 显示判定名称和参与属性（如"说服守卫 - 口才/魅力 判定"）
2. 播放骰子滚动动画（3D 骰子或 2D 翻转效果）
3. 显示骰子点数、属性修正值、最终值、难度
4. 高亮显示成功/失败结果（绿色/红色）
5. 暴击时特殊视觉效果（金色光效/红色震动）
6. 短暂停顿后展示对应分支文本

新增 `UDialogueWidget` 方法：

```cpp
/** 显示属性判定动画和结果 */
UFUNCTION(BlueprintImplementableEvent)
void ShowAttributeCheckResult(const FAttributeCheckResult& Result);

/** 显示属性变化提示 */
UFUNCTION(BlueprintImplementableEvent)
void ShowAttributeChange(const FString& AttributeName, int32 OldValue, int32 NewValue);
```

### 10.3 Blueprint 绑定

`OnAttributeCheckCompleted` 委托绑定到 `DialogueWidget` 的 `ShowAttributeCheckResult` 事件，`OnAttributeChanged` 委托绑定到 `ShowAttributeChange` 事件。

## 11. 开发任务分解

### 阶段一：数据结构与子系统骨架

| 任务 | 文件 | 优先级 |
|------|------|--------|
| 定义属性相关枚举和结构体 | `GameDataTypes.h` | 高 |
| 扩展 `EDialogueActionType` 和 `FDialogueAction` | `GameDataTypes.h` | 高 |
| 扩展 `FWorldConfig` 增加 `PlayerAttributes` | `GameDataTypes.h` | 高 |
| 创建 `UAttributeSystemSubsystem` 头文件 | `Public/AttributeSystemSubsystem.h` | 高 |
| 实现 `InitializeAttributes` / `ModifyAttribute` / `GetAttributeModifier` | `Private/AttributeSystemSubsystem.cpp` | 高 |
| 注册子系统到 `PluginExample.Build.cs`（如需） | `PluginExample.Build.cs` | 高 |

### 阶段二：骰子与判定逻辑

| 任务 | 文件 | 优先级 |
|------|------|--------|
| 实现骰子投掷 `RollDice` | `AttributeSystemSubsystem.cpp` | 高 |
| 实现暴击判定逻辑 | `AttributeSystemSubsystem.cpp` | 高 |
| 实现优势/劣势逻辑 | `AttributeSystemSubsystem.cpp` | 高 |
| 实现属性判定 `ExecuteAttributeCheck`（AND/OR） | `AttributeSystemSubsystem.cpp` | 高 |
| 实现上下文生成 `BuildAttributeContextString` | `AttributeSystemSubsystem.cpp` | 中 |
| 实现 JSON 导入导出 | `AttributeSystemSubsystem.cpp` | 中 |

### 阶段三：LLM 集成

| 任务 | 文件 | 优先级 |
|------|------|--------|
| 扩展 `BuildWorldGenerationSystemPrompt` 增加属性生成 | `ChunkManagerComponent.cpp` | 高 |
| 扩展 `ParseInitialWorldResponse` 解析属性 | `ChunkManagerComponent.cpp` | 高 |
| 扩展 `BuildDialogueSystemPrompt` 增加判定规则 | `ChunkManagerComponent.cpp` | 高 |
| 扩展 `BuildDialogueContextInfo` 注入属性状态 | `ChunkManagerComponent.cpp` | 高 |
| 扩展 `HandleDialogueResponse` 解析判定/修改 | `ChunkManagerComponent.cpp` | 高 |
| 实现 `ExecuteAttributeCheck` 在对话中的调用 | `ChunkManagerComponent.cpp` | 高 |
| 扩展 `ExecuteDialogueActions` 处理新动作类型 | `ChunkManagerComponent.cpp` | 中 |

### 阶段四：UI 集成

| 任务 | 文件 | 优先级 |
|------|------|--------|
| 创建 `WBP_AttributePanel` 蓝图 | `Content/my/UI/` | 中 |
| 扩展 `UDialogueWidget` 增加判定展示接口 | `DialogueWidget.h/.cpp` | 中 |
| 绑定 `OnAttributeCheckCompleted` / `OnAttributeChanged` 委托 | `DialogueWidget.cpp` | 中 |
| 在蓝图中实现骰子滚动动画 | `WBP_Dialogue` 蓝图 | 中 |
| 实现属性变化浮动提示 | `WBP_Dialogue` 蓝图 | 低 |

### 阶段五：测试与调优

| 任务 | 优先级 |
|------|--------|
| 单属性 D20 判定测试（成功/失败/暴击） | 高 |
| 多属性 AND/OR 判定测试 | 高 |
| 优势/劣势骰子测试 | 中 |
| 属性修改钳制测试 | 中 |
| LLM 属性生成格式验证（多世界观） | 中 |
| LLM 判定触发合理性验证 | 中 |
| 存档/读档属性序列化测试 | 低 |

## 12. 注意事项与工程约定

### 12.1 UHT 约束

- 所有 UENUM 值使用英文标识符 + 中文 DisplayName（项目硬约束）
- `TMap<FString, FPlayerAttribute>` 可以标记 UPROPERTY（FPlayerAttribute 是 USTRUCT，非 TPair）
- 委托绑定使用 `AddDynamic`（项目约定）
- 多播委托调用使用 `Broadcast()`（项目约定）
- JSON 反序列化使用 `TSharedRef<TJsonReader<>>` 而非 `TSharedPtr<TJsonReader<>>`（项目约定）

### 12.2 与现有系统的兼容性

- `ParseInitialWorldResponse` 使用 `HasField` 守卫解析新字段，不影响旧存档加载
- `HandleDialogueResponse` 对 `attribute_checks` 字段做可选处理，缺失时按普通对话继续
- `EDialogueActionType` 新增枚举值追加在 `Custom` 之前，保持序列化兼容
- `FDialogueAction` 新增字段使用默认值，不影响现有蓝图引用

### 12.3 性能考虑

- 属性状态注入上下文的字符串控制在 200 字符以内，避免 Token 膨胀
- 骰子计算为纯本地操作，无性能影响
- 判定分支文本由 LLM 一次性返回，避免二次调用延迟
- `AttributeMap` 使用 TMap 查找，O(1) 复杂度

### 12.4 设计决策记录

- **为何将属性子系统独立为 Subsystem 而非放入 ChunkManagerComponent**：属性状态跨对话和场景存在，且未来可能扩展到战斗系统，独立子系统避免 ChunkManager 职责过载

- **为何 LLM 一次性返回成功/失败两套分支**：避免二次 LLM 调用的 2-5 秒延迟，骰子判定是瞬时操作，玩家体验更流畅

- **为何默认 D20 而非 2D6**：D20 的暴击机制（自然 20/1）提供更强的戏剧性，且属性值 1-20 的范围与 D20 天然匹配

- **为何属性修正值用 (value-10)/2 而非直接用属性值**：D&D 5e 的修正值公式使属性差异在骰子判定中不过于主导（+5 修正 vs DC 15 仍有 50% 失败率），保持游戏挑战性

## 13. 开发进度记录

### 2026-07-20 阶段一~三完成

**阶段一：数据结构与子系统骨架** ✅ 完成

| 任务 | 文件 | 状态 |
|------|------|------|
| 定义属性相关枚举和结构体 | `GameDataTypes.h` | ✅ 完成 |
| 扩展 `EDialogueActionType` 和 `FDialogueAction` | `GameDataTypes.h` | ✅ 完成 |
| 扩展 `FWorldConfig` 增加 `PlayerAttributes` | `GameDataTypes.h` | ✅ 完成 |
| 创建 `UAttributeSystemSubsystem` 头文件 | `Public/AttributeSystemSubsystem.h` | ✅ 完成 |
| 实现 `InitializeAttributes` / `ModifyAttribute` / `GetAttributeModifier` | `Private/AttributeSystemSubsystem.cpp` | ✅ 完成 |

**阶段二：骰子与判定逻辑** ✅ 完成

| 任务 | 文件 | 状态 |
|------|------|------|
| 实现骰子投掷 `RollDice` | `AttributeSystemSubsystem.cpp` | ✅ 完成 |
| 实现暴击判定逻辑 | `AttributeSystemSubsystem.cpp` | ✅ 完成 |
| 实现优势/劣势逻辑 | `AttributeSystemSubsystem.cpp` | ✅ 完成 |
| 实现属性判定 `ExecuteAttributeCheck`（AND/OR） | `AttributeSystemSubsystem.cpp` | ✅ 完成 |
| 实现上下文生成 `BuildAttributeContextString` | `AttributeSystemSubsystem.cpp` | ✅ 完成 |
| 实现 JSON 导入导出 | `AttributeSystemSubsystem.cpp` | ✅ 完成 |

**阶段三：LLM 集成** ✅ 完成

| 任务 | 文件 | 状态 |
|------|------|------|
| 扩展 `BuildWorldGenerationSystemPrompt` 增加属性生成 | `ChunkManagerComponent.cpp` | ✅ 完成 |
| 扩展 `ParseInitialWorldResponse` 解析属性 | `ChunkManagerComponent.cpp` | ✅ 完成 |
| 扩展 `BuildDialogueSystemPrompt` 增加判定规则 | `ChunkManagerComponent.cpp` | ✅ 完成 |
| 扩展 `BuildDialogueContextInfo` 注入属性状态 | `ChunkManagerComponent.cpp` | ✅ 完成 |
| 扩展 `HandleDialogueResponse` 解析判定/修改 | `ChunkManagerComponent.cpp` | ✅ 完成 |
| 实现 `ExecuteAttributeCheck` 在对话中的调用 | `ChunkManagerComponent.cpp` | ✅ 完成 |
| 扩展 `ExecuteDialogueActions` 处理新动作类型 | `ChunkManagerComponent.cpp` | ✅ 完成 |

**阶段四：UI 集成** ✅ C++ 完成 / 🔲 蓝图待实现

| 任务 | 文件 | 状态 |
|------|------|------|
| 扩展 `UDialogueWidget` 增加判定展示接口 | `DialogueWidget.h` | ✅ 完成 |
| 绑定 `OnAttributeCheckCompleted` / `OnAttributeChanged` 委托 | `ChunkManagerComponent.cpp` | ✅ 完成 |
| 创建 `UAttributePanelWidget` 基类 | `Public/AttributePanelWidget.h` | ✅ 完成 |
| 创建 `UNotificationWidget` 飘字通知系统 | `Public/NotificationWidget.h` | ✅ 完成 |
| 实现文本式骰子滚动动画 | `DialogueWidget.cpp` | ✅ 完成 |
| 更新 `ExecuteAttributeCheck` 使用动画 | `ChunkManagerComponent.cpp` | ✅ 完成 |
| 创建 `WBP_AttributePanel` 蓝图 | `Content/my/UI/` | 🔲 待实现 |
| 创建 `WBP_Notification` 蓝图 | `Content/my/UI/` | 🔲 待实现 |
| 在蓝图中实现完整骰子动画 | `WBP_Dialogue` 蓝图 | 🔲 待实现 |

### 2026-07-21 UI 系统扩展完成

**新增文件：**

| 文件 | 功能 |
|------|------|
| `Public/NotificationWidget.h` | 飘字通知系统头文件，支持信息、成功、警告、错误、属性变化、道具变化 |
| `Private/NotificationWidget.cpp` | 飘字系统实现，包含淡入/浮动/淡出动画逻辑 |
| `Public/AttributePanelWidget.h` | 属性面板基类，支持按类型分组、实时数据刷新、样式配置 |
| `Private/AttributePanelWidget.cpp` | 属性面板实现，绑定属性子系统事件 |

**修改文件：**

| 文件 | 修改内容 |
|------|----------|
| `DialogueWidget.h` | 新增 `StartDiceRollAnimation`、`UpdateDiceRoll`、`IsDiceRolling`、`GetCurrentDiceValue` |
| `DialogueWidget.cpp` | 实现文本式骰子滚动动画，每帧随机显示数值，1.5秒后显示最终结果 |
| `ChunkManagerComponent.cpp` | 更新 `ExecuteAttributeCheck` 使用骰子动画接口 |

### 实现说明

1. `UAttributeSystemSubsystem` 继承 `UGameInstanceSubsystem`，在 `PluginGameInstance` 初始化时自动创建
2. 属性数据通过 `TMap<FString, FPlayerAttribute>` 存储，按 `AttributeId` 索引
3. 骰子系统使用 `FRandomStream`，支持通过 `SetSeedFromWorldId()` 固定种子以保证存档复现
4. 解析函数（`ParseDiceType`、`ParseAdvantage`、`ParseCheckLogic`、`ParseAttributeType`）均为静态方法，可在 `ParseInitialWorldResponse` 中直接调用
5. 委托绑定使用 `AddDynamic`（项目约定），多播委托调用使用 `Broadcast()`
6. JSON 反序列化使用 `TSharedRef<TJsonReader<>>`（项目约定）
7. 所有 UENUM 值使用英文标识符 + 中文 DisplayName（项目硬约束）
