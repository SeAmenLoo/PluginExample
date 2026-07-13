# PluginExample — AI 驱动 RPG 剧情生成项目

基于 Unreal Engine 5.8 的示例项目，集成 **LLMRuntime** 与 **AIImageGenerator** 两大插件，实现由大语言模型（LLM）实时生成世界观、剧情、场景与 NPC 对话，并由 AI 图像生成服务同步产出角色立绘、像素风行走图及场景/ UI 资源的完整工作流。

---

## 目录

- [项目架构](#项目架构)
- [LLMRuntime 插件](#llmruntime-插件)
- [AIImageGenerator 插件](#aiimagegenerator-插件)
- [AI 生成 RPG 剧情玩法](#ai-生成-rpg-剧情玩法)
  - [玩法简介](#玩法简介)
  - [使用方法](#使用方法)
- [JSON 与配置目录](#json-与配置目录)
- [关键概念与目录约定](#关键概念与目录约定)

---

## 项目架构

```
PluginExample/
├── Source/PluginExample/               # 主游戏模块
│   ├── PluginGameInstance.cpp/h        # 游戏实例：世界生成入口
│   ├── BasePlayerController.cpp/h      # 玩家控制器：UI 流程管理
│   ├── PlayerCharacter.cpp/h           # 玩家角色（3D 角色 + 像素风平面渲染）
│   ├── NPCCharacter.cpp/h              # NPC 角色（交互、对话、像素风展示）
│   ├── ChunkManagerComponent.cpp/h     # 场景管理子系统（世界生成/场景切换/对话调度）
│   ├── CharacterImageSubsystem.cpp/h   # 角色图像管理子系统（LLM × AIImage 交叉协作）
│   ├── DialogueWidget.cpp/h            # 对话 UI
│   ├── WorldSetupWidget.cpp/h          # 世界设定输入 UI
│   └── GameDataTypes.h                 # 核心数据结构（Chunk、NPC、世界配置等）
├── Plugins/
│   ├── LLMRuntime/                     # LLM 运行时插件
│   └── AIImageGenerator/               # AI 图像生成插件
├── Content/                            # 蓝图、材质、动画等资源
└── Saved/Providers/                    # 运行时 provider JSON 配置（高优先级）
```

---

## LLMRuntime 插件

**版本**：0.1.0 | **类型**：Runtime 模块

为 Blueprint 与 C++ 提供统一的运行时 LLM API 访问层，内置对 OpenAI 与 Doubao（火山引擎）服务商的支持。

### 核心能力

| 能力 | 说明 |
|------|------|
| **聊天完成** | `ULLMChatCompletionAsyncAction::ChatCompletion()` 异步调用，支持流式输出（SSE） |
| **多服务商配置** | `ELLMProvider` 枚举支持 OpenAI / Doubao；通过 `ULLMDeveloperSettings` 或运行时 JSON 切换 |
| **JSON Schema 约束** | 内置 `ULLMJsonSchemaDataAsset`，可视化编辑 JSON Schema 字段，强制 LLM 返回结构化数据 |
| **工具调用（Function Calling）** | `FLLMToolDefinition / FLLMToolCall` 支持让 LLM 决策调用游戏内工具 |
| **深度思考模式** | `ELLMThinkingMode` 支持 Default / Disabled / Enabled / Auto，适配 Doubao Seed 等推理模型 |
| **运行时配置覆盖** | `ULLMRuntimeSubsystem` 支持在运行时通过 Blueprint/C++ 覆盖 API Key、BaseUrl、Model 等 |
| **RPG 剧情系统** | 完整的世界观、剧情框架、场景分块、NPC 画像、对话会话与进度追踪子系统 |

### RPG 剧情系统子模块

- **URPGWorldviewDataAsset** — 世界观配置（时代、地理、势力、规则、主题）
- **URPGStoryFrameworkDataAsset** — 剧情框架（主线节点、分支规则、结局条件、主要角色）
- **URPGSceneChunkDataAsset** — 场景分块（NPC 分布、出口点、物品、视觉主题）
- **URPGNPCProfileDataAsset** — NPC 画像（性格、对话风格、知识领域、记忆系统、外观描述）
- **URPGDialogueSubsystem** — 对话会话管理、AI 上下文构建、进度追踪、存档/读档（JSON）
- **URPGBlueprintLibrary** — 提供 Schema 生成、请求构建、响应解析等 Blueprint 工具函数

### 关键 C++ 类型

- `FLLMChatRequest / FLLMChatResponse` — 聊天请求与响应
- `FLLMProviderConfig` — 服务商配置（BaseUrl、ApiKey、Model、Timeout、SchemaConfig）
- `FRPGDialogueSession / FRPGProgressState` — 对话会话与剧情进度

---

## AIImageGenerator 插件

**版本**：0.2.0 | **类型**：Runtime + Editor 双模块

可扩展的 AI 图像生成运行时插件，支持角色图集、场景资源、UI 资产三类生成目标。

### 核心能力

| 能力 | 说明 |
|------|------|
| **角色图集生成** | `UAICharacterImageSetAsyncAction` 一键生成：细节立绘（anchor）、像素风立绘（anchor）、表情图（happy/angry/sad）、行走图（left/right） |
| **场景资源生成** | `UAISceneAssetAsyncAction` 生成 2D 物品、Tile/地面、材质纹理、装饰物、背景层 |
| **UI 资产生成** | `UAIUIAssetAsyncAction` 生成对话框背景、按钮三态、图标、边框、进度条等 |
| **批量生成** | 提供 `AICharacterBatchAsyncAction`、`AISceneBatchAsyncAction`、`AIUIBatchAsyncAction` |
| **多提供商支持** | `EAIImageProviderType` 支持 VolcengineArk、OpenAI Compatible、GenericHttpJson、LocalCommand |
| **运行时配置覆盖** | 与 LLMRuntime 类似，支持运行时 JSON 覆盖图像生成服务商配置 |
| **质量检查** | `AIImageQualityChecker` 自动检查生成结果完整性、文件大小、格式合规 |
| **编辑器工具** | `AIImageGeneratorEditor` 模块提供编辑器 Utility Widget 与资产操作快捷入口 |

### 关键 C++ 类型

- `FAICharacterSpec / FAICharacterImageSet` — 角色规格与生成结果图集
- `FAIImageGenerationRequest / FAIImageGenerationResponse` — 通用图像生成请求与响应
- `FAISceneAssetSpec / FAIUIAssetSpec` — 场景与 UI 资产规格
- `FAIImageProviderConfig` — 图像服务商配置

---

## AI 生成 RPG 剧情玩法

### 玩法简介

1. **世界诞生**  
   玩家在开局 UI 中输入一段世界设定提示词（如：*“一个被永恒寒冬笼罩的蒸汽朋克王国，人民依靠地心热能生存……”*）。`ChunkManagerComponent` 将该提示词发送给 LLM，AI 返回完整的世界观 JSON，并自动生成第一个场景（Chunk）与若干 NPC。

2. **场景探索**  
   每个场景是一个独立的 `AGameChunkActor`，包含地形、NPC、出口传送门（Portal）。玩家通过键盘（WASD）移动，靠近出口可触发场景切换；新场景由 LLM 根据当前世界状态与出口方向实时生成。

3. **AI 自由对话**  
   靠近 NPC 按 **E** 键触发对话。`ChunkManagerComponent` 会将世界观、NPC 人设、对话历史构造成 System Prompt，交由 LLM 实时生成 NPC 回复与可选对话分支。对话历史按 NPC 独立维护，支持跨场景记忆。

4. **角色像素风可视化**  
   `CharacterImageSubsystem` 在检测到新 NPC 或玩家角色时，自动调用 `AIImageGenerator` 生成：
   - **detail_anchor.jpg** — 高细节半身立绘（对话 UI 展示）
   - **pixel_anchor.jpg** — 16bit 像素风全身图（场景中平面渲染）
   - **happy.jpg / angry / sad** — 表情差分（对话 UI 动态切换）
   - **walk_left.jpg / walk_right.jpg** — 行走帧图（场景中移动动画）

5. **剧情进度与存档**  
   `RPGDialogueSubsystem` 维护关键词发现、节点触发、场景访问记录等进度状态，支持导出/导入 JSON 存档。

### 使用方法

#### 1. 启动与配置 Provider

在运行游戏前，需配置 LLM 与图像生成的 API 密钥：

- **方式 A（推荐，便于打包后修改）**：  
  将 `Plugins/LLMRuntime/Config/LLMProvider_template.json` 复制到 `Saved/Providers/LLMProvider.json`，并填入真实 `ApiKey` 与 `Model`。  
  将 `Plugins/AIImageGenerator/Config/IMGProvider_template.json` 复制到 `Saved/Providers/IMGProvider.json`，同样填入密钥。

- **方式 B（编辑器内配置）**：  
  通过 `Project Settings > Plugins > LLM Runtime` 与 `AI Image Generator` 设置 Active Provider Asset。

> 运行时 JSON 配置的优先级高于 Project Settings。

#### 2. 运行游戏流程

1. 点击 Play 后，显示 `WorldSetupWidget`，输入世界设定提示词。  
2. 点击“开始”，游戏显示 Loading 界面，后台并行请求：
   - LLM 生成世界观与初始场景
   - AI 图像生成玩家与 NPC 角色图集  
3. Loading 完成后进入主场景，玩家可：
   - **移动**：WASD 行走，空格跳跃
   - **交互**：靠近 NPC 按 E 打开对话窗口
   - **切换场景**：靠近场景边缘出口自动传送
4. 对话窗口中：
   - 可输入自由文本与 NPC 交谈
   - 或点击预设选项推进剧情
   - 对话可能触发剧情动作（解锁出口、获得物品、改变势力关系等）

#### 3. 在 Blueprint 中调用

```bp
// 发起一次 LLM 聊天（Blueprint 异步节点）
LLM Chat Completion (Request)
  -> Completed (Response)

// 生成角色图集（Blueprint 异步节点）
Generate Character Image Set (Request)
  -> Completed (FAICharacterGenerationResponse)

// 获取对话历史
Chunk Manager Component -> Get Dialogue History
```

#### 4. 在 C++ 中调用

```cpp
// LLM 异步请求
FLLMChatRequest Request;
Request.Messages = ...;
ULLMChatCompletionAsyncAction::ChatCompletion(this, Request);

// AI 图像生成
FAIImageGenerationRequest ImgRequest;
ImgRequest.Prompt = TEXT("...");
UAIImageGenerationAsyncAction::GenerateImage(this, ImgRequest);
```

---

## JSON 与配置目录

### 运行时配置（高优先级）

| 文件路径 | 用途 | 对应插件 |
|----------|------|----------|
| `Saved/Providers/LLMProvider.json` | LLM API 密钥、模型、BaseUrl、Schema 配置 | LLMRuntime |
| `Saved/Providers/IMGProvider.json` | 图像生成 API 密钥、模型、输出格式、超时 | AIImageGenerator |

> 这些文件可在打包后直接修改，无需重新编译。

### 配置模板（插件内参考）

| 模板文件 | 说明 |
|----------|------|
| `Plugins/LLMRuntime/Config/LLMProvider_template.json` | Doubao / OpenAI 通用 LLM 配置模板 |
| `Plugins/LLMRuntime/Config/doubao_provider_template.json` | 火山引擎 Doubao 专用模板 |
| `Plugins/LLMRuntime/Config/openai_provider_template.json` | OpenAI 专用模板 |
| `Plugins/AIImageGenerator/Config/IMGProvider_template.json` | 图像生成通用配置模板 |
| `Plugins/AIImageGenerator/Config/openai_compatible_provider_template.json` | OpenAI Compatible 图像模板 |
| `Plugins/AIImageGenerator/Config/comfyui_provider_template.json` | ComfyUI / LocalCommand 模板 |

### 缓存与输出目录

| 目录 | 用途 |
|------|------|
| `Saved/CharacterImages/{CharacterId}/detail/` | 角色高细节立绘缓存 |
| `Saved/CharacterImages/{CharacterId}/pixel/` | 角色像素风图像缓存 |
| `Saved/CharacterImages/{CharacterId}/bible.json` | 角色设定书（Bible）JSON |
| `Saved/SceneCache/` | 场景资源图像缓存 |
| `Saved/UICache/` | UI 资产图像缓存 |

### 数据结构 JSON 示例

**世界观（URPGWorldviewDataAsset）导出示例**：
```json
{
  "WorldviewId": "world_001",
  "WorldName": "霜铁王国",
  "EraSetting": "蒸汽朋克晚期",
  "GeographyOverview": "被永恒寒冬笼罩的北方大陆...",
  "MajorFactions": [
    {"FactionId": "faction_01", "FactionName": "地热议会", ...}
  ]
}
```

**NPC 画像（URPGNPCProfileDataAsset）导出示例**：
```json
{
  "NPCId": "npc_elara",
  "NPCName": "艾拉娜",
  "NPCRole": "InformationProvider",
  "PersonalityTraits": ["谨慎", "博学", "略带讽刺"],
  "DialogueStyle": {"Tone": "冷静分析", "SpeechPattern": "多用比喻"},
  "MemorySystem": {"RememberPlayerActions": true, "MemoryScope": "Global"}
}
```

---

## 关键概念与目录约定

- **角色图像缓存复用**：所有角色的图像文件名统一（`detail_anchor.jpg`、`happy.jpg`、`pixel_anchor.jpg`、`walk_left.jpg` 等），通过目录 `Saved/CharacterImages/{CharacterId}/` 区分不同角色，便于缓存复用与批量管理。
- **UI 管理分层**：`GameInstance` 负责全局配置与状态，`PlayerController::BeginPlay()` 负责 Widget 创建与 `AddToViewport()`，`ChunkManagerComponent` 负责场景生成与切换。
- **LLM 缓存与回放**：`LLMCacheSubsystem` 支持在重复运行时缓存 LLM 响应，或通过 `bUseCacheReplay` 完全离线回放，节省 Token 与调试时间。
- **JSON 操作规范**：所有 JSON 序列化/反序列化均使用 `TSharedRef<TJsonReader<>>` 与 `TSharedRef<TJsonWriter<>>`，避免使用 `TSharedPtr<TJsonReader<>>` 导致解析异常。
- **UHT 反射限制**：`TPair` 不能标记 `UPROPERTY()`；`BlueprintAssignable` 仅用于 `DECLARE_DYNAMIC_MULTICAST_DELEGATE`；UHT 不支持函数重载与中文枚举标识符（使用英文枚举 + 中文 `DisplayName`）。
