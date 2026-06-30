# LLMRuntime 游戏流程 - 编辑器操作指南

## 目录

1. [概述](#1-概述)
2. [前置条件](#2-前置条件)
3. [配置 LLM Provider](#3-配置-llm-provider)
4. [创建游戏资源](#4-创建游戏资源)
5. [配置 GameInstance](#5-配置-gameinstance)
6. [创建游戏角色](#6-创建游戏角色)
7. [创建 Chunk 预制体](#7-创建-chunk-预制体)
8. [创建 NPC 预制体](#8-创建-npc-预制体)
9. [创建 UI 资源](#9-创建-ui-资源)
10. [运行游戏](#10-运行游戏)
11. [代码结构说明](#11-代码结构说明)

---

## 1. 概述

本系统实现基于 LLM 的 RPG 游戏流程：

- 玩家输入世界观/剧情提示词
- AI 生成初始游戏分块（Chunk）
- AI 基于剧情动态生成新的分块和 NPC
- 玩家可以在分块间移动，与 NPC 对话

## 2. 前置条件

确保以下插件已启用：

1. **LLMRuntime** 插件（位于 `Plugins/LLMRuntime/`）
2. **BlueprintBuster** 插件（可选，用于蓝图调试）

## 3. 配置 LLM Provider

### 3.1 创建 Provider DataAsset

1. 在 Content Browser 中右键
2. 选择 **Miscellaneous → Data Asset**
3. 搜索 `OpenAIProviderDataAsset` 或 `DoubaoProviderDataAsset`
4. 选择并命名（如 `Provider_OpenAI`）

### 3.2 配置 Provider

双击创建的 DataAsset，填写配置：

**OpenAI 配置：**
| 字段 | 值 |
|------|-----|
| Display Name | OpenAI |
| Base Url | `https://api.openai.com` |
| Api Key | `sk-你的密钥` |
| Model | `gpt-4o-mini` |

**Doubao 配置：**
| 字段 | 值 |
|------|-----|
| Display Name | Doubao |
| Base Url | `https://ark.cn-beijing.volces.com` |
| Api Key | 你的 ARK API Key |
| Model | `doubao-seed-1-6-250615` |

### 3.3 注册 Provider

1. 打开 **Edit → Project Settings → LLM Runtime**
2. 在 Provider Registry 中点击 **+** 添加按钮
3. 拖入或选择创建的 Provider DataAsset
4. 设置 **Active Provider Asset** 为该 Provider

## 4. 创建游戏资源

### 4.1 创建 GameInstance

1. 在 Content Browser 中创建文件夹 `GameInstance`
2. 右键 → **Blueprint Class**
3. 搜索 `PluginGameInstance`（C++ 基类）作为父类
4. 命名 `BP_PluginGameInstance`
5. 打开蓝图，在 **Details 面板** 中设置 **Game Config** 分类下的 4 个 Class 属性：

| 属性名 | 选择值 |
|--------|-------|
| World Setup Widget Class | `WB_WorldSetup` |
| Chunk Actor Class | `BP_GameChunk` |
| NPC Character Class | `BP_NPCCharacter` |
| Dialogue Widget Class | `WBP_Dialogue` |

> **注意：** 以上属性在 C++ 基类 `PluginGameInstance` 中声明为 `EditAnywhere, BlueprintReadWrite`，
> 在蓝图的 **Defaults** 面板（点击 Class Defaults 按钮）中直接设置即可，**不需要**在 Event Graph 中手动设置。

### 4.2 配置 Project Settings

1. **Edit → Project Settings → Maps & Modes**
2. 在 **Game Instance Class** 下拉选择 `BP_PluginGameInstance`

## 5. 配置 ChunkManagerComponent

> **重要说明**：`ChunkManagerComponent` 是 `UGameInstanceSubsystem`，**不需要手动添加组件**。它会随 GameInstance 自动实例化。
>
> ChunkManager 的 `ChunkActorClass` / `NPCCharacterClass` / `DialogueWidgetClass` 等属性由
> `PluginGameInstance::Init()` 自动从 GameInstance 的属性同步过去，**不需要单独配置**。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| ChunkActorClass | Class | - | 分块 Actor 类，继承自 `GameChunkActor` |
| NPCCharacterClass | Class | - | NPC 角色类，继承自 `NPCCharacter` |
| DialogueWidgetClass | Class | - | 对话 UI 类，继承自 `DialogueWidget` |
| ChunkSpacing | float | 2000.0 | 分块间距（世界单位） |
| MaxDialogueHistorySize | int | 20 | 最大对话历史消息数 |
| DialogueTemperature | float | 0.8 | 对话生成温度 |
| DialogueMaxTokens | int | 512 | 对话最大 Token 数 |

### 5.2 在其他蓝图中获取 ChunkManagerComponent

**方式一：直接获取 Subsystem**
```
Get Game Instance → Get Subsystem (Class = ChunkManagerComponent)
```

**方式二：使用变量引用**
在 `BP_PluginGameInstance` 中添加变量 `ChunkManager`：
```
Event Init
    ↓
Get Subsystem (Class = ChunkManagerComponent) → Set ChunkManager
```
其他蓝图可从 GameInstance 获取该引用。

### 5.3 代码中获取方式

```cpp
// C++ 中获取 ChunkManagerComponent
#include "ChunkManagerComponent.h"

// 方式1：从 GameInstance
UChunkManagerComponent* ChunkMgr = GameInstance->GetSubsystem<UChunkManagerComponent>();

// 方式2：从 World
UChunkManagerComponent* ChunkMgr = GetWorld()->GetGameInstance()->GetSubsystem<UChunkManagerComponent>();

// 方式3：使用 GameplayStatics
UChunkManagerComponent* ChunkMgr = UGameplayStatics::GetGameInstance(this)->GetSubsystem<UChunkManagerComponent>();
```

## 6. 创建游戏角色

### 6.1 创建玩家角色 (Pawn)

1. 在 Content Browser 中创建文件夹 `Characters/Player`
2. 右键 → **Blueprint Class**
3. 搜索 `Pawn` 作为父类
4. 命名 `BP_PlayerCharacter`

**组件配置：**
- 添加 `CapsuleComponent`（碰撞）
- 添加 `CharacterMovementComponent`
- 添加 `CameraComponent`（第三人称视角）

### 6.2 配置 Input

在 Project Settings → Input 中添加：

| Action | Key |
|--------|-----|
| MoveForward | W |
| MoveRight | A/S/D |
| Jump | Space |
| Interact | E |

## 7. 创建 Chunk 预制体

### 7.1 创建 GameChunk Actor

1. 在 Content Browser 中创建文件夹 `Chunks`
2. 右键 → **Blueprint Class**
3. 搜索 `GameChunkActor`（C++ 基类）作为父类
4. 命名 `BP_GameChunk`

**组件配置：**
- 在 **Details** 中设置默认 ChunkSize（如 1000.0）
- 配置 ChunkMesh（可选，使用默认平面或自定义模型）
- 设置 ChunkNameLabel 的样式

### 7.2 创建关卡

1. 创建多个 Level（如 `Level_Forest`、`Level_Village`）
2. 在每个关卡中放置 `BP_GameChunk` 实例
3. 配置各分块的 ExitPoints

## 8. 创建 NPC 预制体

### 8.1 创建 NPC Character

1. 在 Content Browser 中创建文件夹 `NPCs`
2. 右键 → **Blueprint Class**
3. 搜索 `NPCCharacter`（C++ 基类）作为父类
4. 命名 `BP_NPCCharacter`

**组件配置：**
- 设置 **InteractionSphere** 的半径（如 150.0）
- 配置 NPCNameLabel 的样式
- 设置 InteractionKey 为 `E`

### 8.2 创建 NPC 行为

可以为 NPC 添加：
- 巡逻路径（Patrol Points）
- 动画状态机
- 对话触发器

## 9. 创建 UI 资源

### 9.1 创建 WorldSetupWidget

1. 在 Content Browser 中创建文件夹 `UI`
2. 右键 → **User Interface → Widget Blueprint**
3. 命名 `WB_WorldSetup`

**设计布局：**
```
+----------------------------------+
|       World Setup                |
|                                  |
|  Game Style: [Dropdown    v]     |
|                                  |
|  World Description:              |
|  [                          ]   |
|  [                          ]   |
|                                  |
|  Player Name: [             ]    |
|                                  |
|       [Start Game]              |
+----------------------------------+
```

**绑定事件：**
- StartGameButton → OnClicked → 调用 `OnWorldSetupComplete` 委托

### 9.1.1 WorldSetupWidget 蓝图节点连接图

**节点图：OnClicked 事件**

```
┌─────────────────────────────────────────────────────────────────┐
│  StartGameButton (OnClicked)                                    │
│  ────► [Get World Description from WorldPromptTextBox]          │
│           │                                                      │
│           ▼                                                      │
│     [Get Player Name from PlayerCharacterNameBox]               │
│           │                                                      │
│           ▼                                                      │
│     [Get Game Style from GameStyleSelector]                     │
│           │                                                      │
│           ▼                                                      │
│     [Format String: "Create a {Style} world with details: {Desc}"│
│      + " Player name: {Name}"]                                   │
│           │                                                      │
│           ▼                                                      │
│     [Create Prompt String]                                      │
│           │                                                      │
│           ▼                                                      │
│     [Call OnWorldSetupComplete.Broadcast(Prompt)]              │
│           │                                                      │
│           ▼                                                      │
│     [Remove from Parent]                                        │
└─────────────────────────────────────────────────────────────────┘
```

**详细节点步骤：**

| 步骤 | 节点 | 输入 | 输出 |
|------|------|------|------|
| 1 | **Event OnClicked** (Button) | - | - |
| 2 | **Get Text** (WorldPromptTextBox) | - | Text |
| 3 | **Get Text** (PlayerCharacterNameBox) | - | Text |
| 4 | **Get Selected Option** (GameStyleSelector) | - | String |
| 5 | **Format String** | Style, Desc, Name | Combined String |
| 6 | **OnWorldSetupComplete.Broadcast** | WorldPrompt | - |
| 7 | **Remove from Parent** | - | - |

**蓝图事件图代码（伪代码）：**

```
Event OnClicked (from StartGameButton):
    → GetText (WorldPromptTextBox)
    → GetText (PlayerCharacterNameBox)
    → GetSelectedOption (GameStyleSelector)
    → Format String: "Create a {0} style world with: {1}. Player name: {2}"
        Arg 0 = GameStyle
        Arg 1 = WorldDescription
        Arg 2 = PlayerName
    → OnWorldSetupComplete.Broadcast(Result)
    → RemoveFromParent()
```

---

### 9.2 创建 DialogueWidget

1. 在 Content Browser 中创建文件夹 `UI`
2. 右键 → **User Interface → Widget Blueprint**
3. 命名 `WBP_Dialogue`

**设计布局：**
```
+----------------------------------+
|  [NPC Portrait]                  |
|                                  |
|  NPC Name                        |
|  --------------------------------|
|  Dialogue text goes here...       |
|                                  |
|  [Choice 1]                      |
|  [Choice 2]                      |
|  [Choice 3]                      |
|                                  |
|       [Continue]                 |
+----------------------------------+
```

**绑定组件：**
- NPCNameText (TextBlock)
- HistoryScrollBox (ScrollBox)
- HistoryContainer (VerticalBox)
- DialogueText (TextBlock)
- ChoicesBox (VerticalBox)
- ChoicesContainer (SizeBox)
- ContinueButton (Button)
- InputTextBox (EditableTextBox)
- SendButton (Button)
- InputContainer (SizeBox)

### 9.2.1 DialogueWidget 蓝图节点连接图

#### 节点图 1：Native Construct（初始化）

```
┌──────────────────────────────────────────────────────────────┐
│  Event NativeConstruct                                       │
│  ───► [Set InputContainer Visibility: Collapsed]             │
│         │                                                     │
│         ▼                                                     │
│     [Set ChoicesContainer Visibility: Collapsed]             │
│         │                                                     │
│         ▼                                                     │
│     [Set HistoryScrollBox Visibility: Visible]               │
│         │                                                     │
│         ▼                                                     │
│     [Set DialogueText Text: ""]                              │
└──────────────────────────────────────────────────────────────┘
```

#### 节点图 2：Continue Button OnClicked

```
┌──────────────────────────────────────────────────────────────┐
│  ContinueButton (OnClicked)                                  │
│  ───► [Branch: Is Typewriter Active?]                        │
│         │                                                     │
│         ├── True ──► [Skip Typewriter → Show Full Text]     │
│         │              │                                      │
│         │              ▼                                      │
│         │         [Show Choices if any]                      │
│         │              │                                      │
│         │              ▼                                      │
│         │         [OnTypewriterComplete (Event)]            │
│         │                                                     │
│         └── False ──► [Branch: Input Visible?]               │
│                              │                                 │
│                              ├── False ──► [Show Input Field] │
│                              │                                 │
│                              └── True ──► [Hide Input Field]  │
└──────────────────────────────────────────────────────────────┘
```

#### 节点图 3：Send Button OnClicked

```
┌──────────────────────────────────────────────────────────────┐
│  SendButton (OnClicked)                                      │
│  ───► [Get Text from InputTextBox]                           │
│         │                                                     │
│         ▼                                                     │
│     [Is Text Empty?] ──► [Branch]                            │
│         │                     │                               │
│         │                     ├── True ──► [Do Nothing]      │
│         │                     │                               │
│         │                     └── False ──►                   │
│         │                                   │                  │
│         ▼                                   ▼                  │
│     [AddMessageToHistory("You", Text, true)]                 │
│         │                                                     │
│         ▼                                                     │
│     [OnMessageSubmittedDelegate.Broadcast(Text)]             │
│         │                                                     │
│         ▼                                                     │
│     [Clear Input Text]                                       │
│         │                                                     │
│         ▼                                                     │
│     [Hide Input Field]                                       │
└──────────────────────────────────────────────────────────────┘
```

#### 节点图 4：InputTextBox OnTextCommitted

```
┌──────────────────────────────────────────────────────────────┐
│  InputTextBox (OnTextCommitted)                              │
│  ───► [Branch: Commit Type == OnEnter?]                     │
│         │                                                     │
│         └── True ──► [Call SendButton OnClicked]            │
└──────────────────────────────────────────────────────────────┘
```

#### 节点图 5：Choice Button 点击（动态生成）

> 注意：选项按钮由 C++ 动态创建，点击事件在 C++ 中处理。
> 如果需要在蓝图中处理，可绑定 `OnChoiceSelectedDelegate`：

```
┌──────────────────────────────────────────────────────────────┐
│  Event OnChoiceSelectedDelegate                              │
│  ───► [Print String: "Choice selected: {Index}"]            │
│         │                                                     │
│         ▼                                                     │
│     [Play Sound: Click Sound]                                │
└──────────────────────────────────────────────────────────────┘
```

### 9.2.2 DialogueWidget 完整事件一览

| 事件 | 触发时机 | 处理逻辑 |
|------|---------|---------|
| **NativeConstruct** | Widget 创建时 | 隐藏输入框，初始化状态 |
| **ContinueButton.OnClicked** | 点击继续按钮 | 跳过打字机 / 显示输入框 |
| **SendButton.OnClicked** | 点击发送按钮 | 发送文本消息 |
| **InputTextBox.OnTextCommitted** | 输入框回车 | 发送文本消息 |
| **OnChoiceSelectedDelegate** | 选择对话选项 | 触发选项选择逻辑 |
| **OnMessageSubmittedDelegate** | 文本消息提交 | 触发消息发送逻辑 |
| **OnDialogueClosedDelegate** | 关闭对话 | 移除 Widget |
| **OnTypewriterComplete** | 打字机效果完成 | 显示选项 / 输入框 |
| **OnMessageAdded** | 新消息添加 | 滚动到底部 |

### 9.2.3 C++ 中动态创建选项按钮的节点

C++ 中 `ShowChoices()` 方法动态创建按钮，等效蓝图节点：

```
For Loop (i = 0 to Choices.Num()):
    ├─► Create Widget (Button)
    ├─► Create Widget (TextBlock)
    ├─► Set Text of TextBlock = Choices[i]
    ├─► Add Child (Button → TextBlock)
    ├─► Add Child (ChoicesBox → Button)
    └─► Bind Button.OnClicked → SelectChoice(i)
```

## 10. 运行游戏

### 10.1 完整游戏加载流程

```
游戏启动
    ↓
UE4 引擎初始化
    ↓
加载 Game Instance (BP_PluginGameInstance)
    ↓
调用 GameInstance::Init()
    │
    ├─► 获取 ChunkManagerComponent (Subsystem 自动实例化)
    │
    ├─► 加载 BP_GameChunk / BP_NPCCharacter / WBP_Dialogue 类
    │   并配置到 ChunkManagerComponent
    │
    ├─► 加载 WB_WorldSetup 蓝图
    │   └─► 创建 Widget 实例
    │   └─► AddToViewport
    │   └─► 绑定 OnWorldSetupComplete → StartGameWithPrompt
    │
    └─► 绑定 ChunkManager 事件
        ├─► OnWorldGenerationComplete → OnWorldGenerationComplete
        └─► OnWorldGenerationFailed → OnWorldGenerationFailed
    ↓
显示 World Setup 界面（玩家输入提示词）
    ↓
玩家点击 Start Game
    ↓
StartGameWithPrompt()
    ├─► 移除 WorldSetupWidget
    └─► ChunkManager::GenerateWorldFromPrompt()
        └─► 调用 LLM API (ChatCompletion)
    ↓
LLM 返回世界配置 JSON
    ↓
解析 JSON → FWorldConfig + FChunkData (初始分块)
    ↓
SpawnChunkAndNPCs()
    ├─► Spawn BP_GameChunk
    ├─► Spawn BP_NPCCharacter (1-2个)
    └─► 添加到 LoadedChunks
    ↓
OnWorldGenerationComplete.Broadcast()
    ↓
TransitionToGame()
    ├─► SetInputMode(FInputModeGameOnly)
    └─► 隐藏鼠标
    ↓
游戏开始！玩家可自由探索
    ↓
玩家走到分块出口
    ↓
ChunkManager::TryTransitionToNextChunk()
    ├─► 检查目标分块是否已加载
    ├─► 已加载 → 直接传送
    └─► 未加载 → 调用 LLM 生成新分块
    ↓
玩家按 E 与 NPC 对话
    ↓
PlayerCharacter::OnInteractPressed()
    └─► ChunkManager::StartDialogue(NPC)
        ├─► 创建 DialogueWidget
        ├─► 绑定委托
        ├─► SendDialogueRequest() → LLM
        └─► 显示打字机效果 + 对话选项
```

### 10.2 初始场景（Startup Level）放哪些 Actor？

**场景路径：** `/Game/Maps/StartupMap`

**必须放置的 Actor：**

| Actor 类型 | 数量 | 说明 |
|-----------|------|------|
| **Player Start** | 1 | 玩家出生点 |
| **Sky Atmosphere** | 1 | 天空大气 |
| **Directional Light** | 1 | 主光源（太阳光） |
| **Exponential Height Fog** | 1 | 雾效 |
| **Static Mesh Actor (Floor)** | 1 | 地面（测试用） |

> **重要：** 初始场景不需要放任何 Chunk 或 NPC！
> 它们会由 ChunkManagerComponent 在游戏运行时动态生成。

**可选放置的 Actor：**
- 天空球（Sky Sphere）
- 大气雾
- 后期处理体积（Post Process Volume）

### 10.3 必须创建的蓝图资源清单

| 蓝图类型 | 蓝图名 | 父类（C++） | 存放路径 | 必须 |
|---------|-------|------------|---------|------|
| GameInstance | `BP_PluginGameInstance` | `PluginGameInstance` | `GameInstance/` | ✅ |
| Player Controller | `BP_BasePlayerController` | `BasePlayerController` | `Characters/Player/` | ✅ |
| Player Character | `BP_PlayerCharacter` | `PlayerCharacter` | `Characters/Player/` | ✅ |
| Game Mode | `BP_GameMode` | `GameModeBase` | `GameMode/` | ✅ |
| Game Chunk Actor | `BP_GameChunk` | `GameChunkActor` | `Chunks/` | ✅ |
| NPC Character | `BP_NPCCharacter` | `NPCCharacter` | `NPCs/` | ✅ |
| Widget (World Setup) | `WB_WorldSetup` | `WorldSetupWidget` | `UI/` | ✅ |
| Widget (Dialogue) | `WBP_Dialogue` | `DialogueWidget` | `UI/` | ✅ |

### 10.4 必须设置的配置项

#### Project Settings 配置

| 设置项 | 路径 | 值 |
|--------|------|-----|
| **Game Instance Class** | Edit → Project Settings → Maps & Modes → Game Instance | `BP_PluginGameInstance` |
| **Default GameMode** | Edit → Project Settings → Maps & Modes → Default GameMode | `BP_GameMode` |
| **Startup Map** | Edit → Project Settings → Maps & Modes → Default Maps → Game Default Map | `StartupMap` |
| **LLM Provider** | Edit → Project Settings → LLM Runtime → Active Provider Asset | 配置好的 Provider DataAsset |

#### BP_GameMode 配置

| 属性 | 值 |
|------|-----|
| **Default Pawn Class** | `BP_PlayerCharacter` |
| **Player Controller Class** | `BP_BasePlayerController` |
| **HUD Class** | `HUD` (默认即可) |

#### BP_PlayerCharacter 配置

| 属性 | 值 |
|------|-----|
| Auto Possess Player | Player 0 |
| Interaction Radius | 200.0 |

### 10.5 World Setup Widget 在哪里打开？

**答案：在 `ABasePlayerController::BeginPlay()` 中自动打开**

见 [BasePlayerController.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Private/BasePlayerController.cpp)

**为什么不在 GameInstance 里打开？**
- `GameInstance::Init()` 调用时 World 和 Viewport 还没准备好
- `AddToViewport()` 需要有效的 Player 和 Viewport
- PlayerController 是 UI 的正确宿主

**完整调用链：**

```
游戏启动
    ↓
GameInstance::Init()
    └─ 配置 ChunkManager (不创建 UI)
    ↓
Level 加载完成
    ↓
PlayerController::BeginPlay()
    ├─ 获取 GameInstance 和 ChunkManager
    ├─ 创建 WorldSetupWidget
    ├─ AddToViewport
    └─ 切换到 UIOnly 输入模式
    ↓
显示 World Setup 界面
```

**子蓝图创建位置：**
```
Content Browser → UI 文件夹 → 右键 → User Interface → Widget Blueprint
    → 命名为 WB_WorldSetup
    → 打开 Widget Blueprint Editor
    → 在 Graph 面板中 → File → Reparent Blueprint → 选择 WorldSetupWidget
```

### 10.6 各系统职责划分

| 系统 | 类 | 职责 |
|------|----|------|
| **全局状态** | UPluginGameInstance | 存配置、管理 ChunkManager、跨关卡状态 |
| **UI 管理** | ABasePlayerController | 创建/管理所有 UI Widget、输入模式切换 |
| **世界管理** | UChunkManagerComponent (Subsystem) | 分块生成、NPC生成、对话管理 |
| **玩家角色** | APlayerCharacter | 移动、视角、交互输入 |
| **NPC** | ANPCCharacter | NPC 表现、碰撞检测 |

### 10.7 启动游戏

1. 确保已编译 C++ 代码
2. 打开编辑器，打开 `StartupMap`
3. 点击 **Play** 按钮
4. **World Setup 界面自动弹出**
5. 输入：
   - 游戏风格（Medieval Fantasy, Sci-Fi, etc.）
   - 世界描述（如 "一个剑与魔法的中世纪世界，有精灵和矮人"）
   - 玩家角色名
6. 点击 **Start Game**
7. 等待 AI 生成初始分块（几秒）
8. 生成完成后自动进入游戏世界

### 10.7 游戏内操作

| 操作 | 键位 |
|------|------|
| 移动 | WASD |
| 跳跃 | Space |
| 视角 | 鼠标 |
| 与 NPC 对话 | E |
| 退出对话 | 点击 Continue 或发送消息 |

### 10.8 出口移动

走到分块边缘的出口点（ExitPoint），系统会自动：
1. 检查目标分块是否已加载
2. 若未加载，调用 AI 生成新分块
3. 传送玩家到新分块

## 11. LLM API 与日志系统

### 11.1 LLMRuntime 插件可用的 API

LLMRuntime 插件目前只提供一个核心 API：

| API | 类型 | 说明 |
|-----|------|------|
| **Chat Completion** | 异步（AsyncAction） | 聊天补全，支持 JSON Schema、Function Calling |

**调用方式：**
```cpp
#include "LLMRuntime/Public/LLMChatCompletionAsyncAction.h"

// 创建请求
FLLMChatRequest Request;
Request.Messages = Messages;
Request.Temperature = 0.7f;
Request.MaxTokens = 2048;
Request.bForceJsonObject = true;

// 调用
ULLMChatCompletionAsyncAction* Action = ULLMChatCompletionAsyncAction::ChatCompletion(this, Request);
Action->Completed.AddDynamic(this, &UMyClass::HandleResponse);
```

**相关蓝图节点：**
| 节点 | 说明 |
|------|------|
| `Chat Completion` | 异步聊天请求 |
| `Make Chat Message` | 构造消息 |
| `Make Function Tool` | 创建函数工具 |
| `Make Json Schema System Message` | 创建 JSON Schema 消息 |
| `Build Game Config Generation Request` | 构建游戏配置生成请求 |
| `Parse Response To Game Config` | 解析响应到 DataAsset |

### 11.2 LLM 日志系统（ULLMLoggerSubsystem）

项目侧添加了 `ULLMLoggerSubsystem`，自动记录所有 LLM 请求和响应。

**日志输出通道：** `LogPluginLLM`

**日志级别与内容：**
| 级别 | 内容 |
|------|------|
| `Log` | 请求摘要（消息数、token、耗时）、响应摘要 |
| `Verbose` | 消息摘要（角色+前100字符） |
| `VeryVerbose` | 完整消息内容、完整响应内容 |
| `Error` | 错误信息 |

**在 Output Log 中过滤：**
```
LogPluginLLM
```

### 11.3 日志详细程度控制

```cpp
// 获取 Logger
ULLMLoggerSubsystem* Logger = GetGameInstance()->GetSubsystem<ULLMLoggerSubsystem>();

// 0 = 仅统计（请求数、token）
// 1 = 正常（默认，摘要信息）
// 2 = 详细（完整消息内容）
// 3 = 调试（全部输出）
Logger->SetVerbosity(1);
```

### 11.4 请求类型分类

| 类型 | 枚举值 | 触发时机 |
|------|--------|---------|
| 世界生成 | `WorldGeneration` | `GenerateWorldFromPrompt()` |
| 分块生成 | `ChunkGeneration` | `GenerateNextChunk()` |
| NPC 对话 | `NPCDialogue` | `SendDialogueRequest()` |
| 其他 | `Other` | 自定义使用 |

### 11.5 手动记录日志

```cpp
#include "LLMLoggerSubsystem.h"

ULLMLoggerSubsystem* Logger = GetGameInstance()->GetSubsystem<ULLMLoggerSubsystem>();

// 记录请求
Logger->LogRequest(ELLMRequestType::Other, Request, TEXT("my_context"));

// 记录响应
Logger->LogResponse(ELLMRequestType::Other, Response, DurationSeconds, TEXT("my_context"));

// 记录错误
Logger->LogError(ELLMRequestType::Other, TEXT("Something went wrong"));
```

### 11.6 会话统计

```cpp
// 总请求数
int32 Total = Logger->GetTotalRequests();

// 累计 Token 使用
FLLMUsage Usage = Logger->GetTotalUsage();
int32 PromptTokens = Usage.PromptTokens;
int32 CompletionTokens = Usage.CompletionTokens;
int32 TotalTokens = Usage.TotalTokens;
```

统计会在 `Deinitialize` 时自动打印到日志：
```
LogPluginLLM: === LLM Session Summary ===
LogPluginLLM: Total Requests: 15
LogPluginLLM: Success: 12 | Errors: 3
LogPluginLLM: Total Tokens - Prompt: 4521, Completion: 892, Total: 5413
LogPluginLLM: ============================
```

## 12. 代码结构说明

### 12.1 新增 C++ 类

| 类名 | 路径 | 说明 |
|------|------|------|
| `FChunkData` | Public/GameDataTypes.h | 分块数据结构 |
| `FNPCData` | Public/GameDataTypes.h | NPC 数据结构 |
| `FExitPoint` | Public/GameDataTypes.h | 出口点数据结构 |
| `FWorldConfig` | Public/GameDataTypes.h | 世界配置结构 |
| `AGameChunkActor` | Public/GameChunkActor.h | 分块 Actor |
| `ANPCCharacter` | Public/NPCCharacter.h | NPC 角色 |
| `UChunkManagerComponent` | Public/ChunkManagerComponent.h | 分块管理器（GameInstanceSubsystem） |
| `UDialogueWidget` | Public/DialogueWidget.h | 对话 UI 组件 |
| `UWorldSetupWidget` | Public/WorldSetupWidget.h | 世界设置 UI |
| `UPluginGameInstance` | Public/PluginGameInstance.h | 游戏实例 |

### 11.2 依赖关系

```
UPluginGameInstance
    └── UChunkManagerComponent
            ├── AGameChunkActor (Spawn)
            └── ANPCCharacter (Spawn)

UDialogueWidget ← BP_Blueprint
UWorldSetupWidget ← BP_Widget

ANPCCharacter
    └── UDialogueWidget (Spawn on dialogue start)
```

### 11.3 数据流

```
Player Input (Prompt)
        ↓
UChunkManagerComponent::GenerateWorldFromPrompt()
        ↓
LLMRuntime API (ChatCompletion)
        ↓
Parse JSON Response
        ↓
Spawn AGameChunkActor + ANPCCharacter
        ↓
OnWorldGenerationComplete.Broadcast()
        ↓
TransitionToGame()
```

### 11.4 LLM 提示词模板

**世界生成提示词** (`BuildWorldGenerationSystemPrompt`):
- 生成包含 initial_chunk 的世界配置
- 初始分块需要 1-2 个 NPC
- 初始分块需要 2-4 个出口点

**分块生成提示词** (`BuildChunkGenerationSystemPrompt`):
- 基于当前剧情生成新分块
- 包含 NPC 和出口点

**对话生成提示词** (`BuildDialogueSystemPrompt`):
- NPC 回复
- 对话选项

---

## 常见问题

### Q: 编译报错找不到头文件

**A:** 确保 LLMRuntime 插件已正确编译。检查：
1. 插件已启用
2. `.uproject` 文件中插件已注册
3. 运行 `Generate Project Files`

### Q: AI 生成失败

**A:** 检查：
1. Provider DataAsset 的 API Key 是否正确
2. 网络连接是否正常
3. 查看 Output Log 中的错误信息

### Q: 玩家无法移动到新分块

**A:** 检查：
1. ExitPoint 的 Collision 设置为 Overlap
2. ChunkManagerComponent 已正确配置 ChunkActorClass
3. 玩家 Pawn 继承自 APawn

---

## 12. 增强对话系统功能

### 12.1 功能概述

增强后的对话系统支持：

| 功能 | 说明 |
|------|------|
| **对话历史显示** | 滚动显示所有对话内容 |
| **打字机效果** | NPC 对话逐字显示 |
| **自由文本输入** | 玩家可输入任意文字 |
| **选项选择** | 玩家可点击 AI 生成的选项 |
| **多轮对话上下文** | LLM 记住对话历史 |
| **Token 优化** | 自动裁剪过长历史 |

### 12.2 DialogueWidget 布局设计

```
+----------------------------------+
|  NPC Name                        |
|  --------------------------------|
|  +----------------------------+  |
|  | History ScrollBox          |  |
|  | - Player: Hello           |  |
|  | - NPC: Hi there!          |  |
|  | - Player: How are you?    |  |
|  | - NPC: I'm fine...        |  |
|  +----------------------------+  |
|                                  |
|  Current Dialogue Text           |
|  (with typewriter effect)       |
|                                  |
|  +----------------------------+  |
|  | ChoicesBox                 |  |
|  | [1. Option 1]             |  |
|  | [2. Option 2]             |  |
|  +----------------------------+  |
|                                  |
|  +----------------------------+  |
|  | InputContainer             |  |
|  | [InputTextBox] [SendBtn]   |  |
|  +----------------------------+  |
|                                  |
|       [Continue]                 |
+----------------------------------+
```

### 12.3 必需的 BindWidget 组件

| 变量名 | 类型 | 说明 |
|--------|------|------|
| `NPCNameText` | TextBlock | NPC 名称显示 |
| `HistoryScrollBox` | ScrollBox | 对话历史滚动框 |
| `HistoryContainer` | VerticalBox | 历史消息容器 |
| `DialogueText` | TextBlock | 当前对话文本（打字机效果） |
| `ChoicesBox` | VerticalBox | 选项按钮容器 |
| `ChoicesContainer` | SizeBox | 选项容器（控制显隐） |
| `ContinueButton` | Button | 继续/跳过按钮 |
| `InputTextBox` | EditableTextBox | 玩家输入框 |
| `SendButton` | Button | 发送按钮 |
| `InputContainer` | SizeBox | 输入容器（控制显隐） |

### 12.4 委托和事件

| 委托 | 参数 | 触发时机 |
|------|------|---------|
| `OnMessageSubmittedDelegate` | `FString` (消息文本) | 玩家提交文本输入 |
| `OnChoiceSelectedDelegate` | `int32` (选项索引) | 玩家点击选项 |
| `OnDialogueClosedDelegate` | - | 对话关闭 |
| `OnTypewriterComplete` | - | 打字机效果完成 |
| `OnMessageAdded` | - | 新消息添加到历史 |

### 12.5 ChunkManagerComponent 对话配置

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `MaxDialogueHistorySize` | int32 | 20 | 最大对话历史消息数 |
| `DialogueTemperature` | float | 0.8f | 对话生成温度 |
| `DialogueMaxTokens` | int32 | 512 | 对话最大 Token 数 |

### 12.6 多轮对话数据流

```
1. 玩家按 E 开始对话
        ↓
2. StartDialogue() - 创建 Widget，绑定委托
        ↓
3. SendDialogueRequest() - 发送 LLM 请求
        ↓
4. LLM 返回 response_text + choices
        ↓
5. ShowNPCResponse() - 显示打字机效果
        ↓
6. 打字机完成后显示选项 / 输入框
        ↓
7. 玩家输入文本或选择选项
        ↓
8. SubmitPlayerTextInput() / SubmitPlayerChoice()
        ↓
9. AddDialogueMessage() - 添加到历史
        ↓
10. SendDialogueRequest() - 发送下一轮请求
```

### 12.7 LLM 对话响应格式

```json
{
    "response_text": "NPC说的话，1-3句，保持角色",
    "choices": ["选项1", "选项2", "选项3"]
}
```

- `choices` 可以是空数组 `[]`（纯自由输入模式）
- `choices` 通常 2-4 个选项

### 12.8 对话历史维护

系统自动维护对话历史并发送给 LLM：

```
Messages = [
    {role: "system", content: "[角色设定提示词]"},
    {role: "system", content: "[游戏状态上下文]"},
    {role: "user", content: "玩家说的话1"},
    {role: "assistant", content: "NPC的回复1"},
    {role: "user", content: "玩家说的话2"},
    {role: "assistant", content: "NPC的回复2"},
    ...
]
```

超过 `MaxDialogueHistorySize` 时自动裁剪早于 `MaxDialogueHistorySize-1` 的消息。
