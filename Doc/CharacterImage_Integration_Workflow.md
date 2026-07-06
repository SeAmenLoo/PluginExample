# 角色图生成集成流程

本文档说明将 AIImageGenerator 角色图生成集成到游戏对话流程中的完整操作步骤。
代码部分已在以下位置实现：
- 子系统：[CharacterImageSubsystem.h](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Public/CharacterImageSubsystem.h) / [CharacterImageSubsystem.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Private/CharacterImageSubsystem.cpp)
- NPC Flipbook 组件：[NPCCharacter.h](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Public/NPCCharacter.h) / [NPCCharacter.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Private/NPCCharacter.cpp)
- 玩家 Flipbook 组件：[PlayerCharacter.h](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Public/PlayerCharacter.h) / [PlayerCharacter.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Private/PlayerCharacter.cpp)
- 对话框 UI Image：[DialogueWidget.h](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Public/DialogueWidget.h) / [DialogueWidget.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Private/DialogueWidget.cpp)
- Chunk 集成：[ChunkManagerComponent.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Private/ChunkManagerComponent.cpp)（SpawnChunkAndNPCs + StartDialogue 处）
- 世界生成完成集成：[BasePlayerController.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Private/BasePlayerController.cpp)（OnWorldGenerationComplete 处）

---

## 1. 代码已自动处理的部分

| 步骤 | 说明 | 是否需要手动处理 |
|------|------|------------------|
| CharacterImageSubsystem 初始化 | GameInstanceSubsystem，自动创建 | ❌ |
| 监听 NPC 生成 | `UChunkManagerComponent::SpawnChunkAndNPCs` 已调用 `RequestCharacterImageForNPC` | ❌ |
| 异步请求图集 | 调用 `UAICharacterImageSetAsyncAction::GenerateCharacterImageSet` | ❌ |
| 内存缓存（`CachedDetailFlipbooks` / `CachedPixelFlipbooks` / `CachedPortraitTextures`） | 按 NPCId 缓存 | ❌ |
| 磁盘缓存（`Saved/CharacterImages/{NPCId}/*.png`） | 由 AIImageGenerator 写入 | ❌ |
| NPC 第二次启动时跳过请求 | 通过 `HasCachedImageSet` 判断 | ❌ |
| 自动应用到 NPC `UPaperFlipbookComponent` | 通过 `ANPCCharacter::SetPixelFlipbook` | ❌ |
| 自动应用到 DialogueWidget 的 `NPCPortraitImage` | 通过 `UDialogueWidget::SetNPCPortrait` | ❌ |
| 在 `StartDialogue` 中复用已缓存图像 | `ChunkManagerComponent` 已调用 `ApplyCachedImageToDialogue` | ❌ |
| 玩家角色图在世界生成完成后请求 | `ABasePlayerController::OnWorldGenerationComplete` 已调用 `RequestPlayerCharacterImage` | ❌ |
| 玩家角色图缓存/复用 | 使用 `CharacterId = "Player"` 复用同一套缓存逻辑 | ❌ |

---

## 2. 需要手动处理的部分

### 2.1 创建 AIImageProvider 数据资产

1. 在 UE 编辑器中打开 Content Browser，定位到 `Content/AIImage/` 文件夹（若不存在则新建）。
2. 右键 → `Miscellaneous` → `Data Asset` → 搜索 `AIImageProviderConfig`。
3. 命名建议：`DA_AIImageProvider_Primary`。
4. 打开资产，在 `ProviderType` 中选择 `VolcengineArk`（或 `OpenAICompatible`）。
5. 在 `BaseUrl` / `ApiKey` / `Model` 字段填入对应提供商的配置。
6. 打开 `Project Settings` → `AI Image Generator` → `Active Provider Config`，引用刚才创建的数据资产。

> **注意**：如果直接使用火山引擎的 ARK，不需要额外环境变量。如果使用其他 provider，参考 [AIImageGenerator 插件文档](file:///e:/Projects/UE/VRGame/git/PluginExample/Doc/AI_UIImage_Generation_Design.md) 配置。

### 2.2 在 MCP 编辑器中给 NPC 蓝图（BP_NPCCharacter）正确暴露组件

代码已经创建了：
- `DetailFlipbookComp` (UPaperFlipbookComponent)
- `PixelFlipbookComp` (UPaperFlipbookComponent)

你需要：
1. 打开 `Content/NPCs/BP_NPCCharacter`（或继承自 `ANPCCharacter` 的蓝图）。
2. 在 `Components` 面板应看到 `DetailFlipbookComp` 和 `PixelFlipbookComp`。
3. 调整它们的相对位置（`RelativeLocation` / `RelativeRotation`），让锚点对准角色脚底。
4. 设置 `bUsePixelArt = true`（默认即 true）以使 `PixelFlipbookComp` 可见。

> 运行时图集生成后，代码会自动调用 `SetDetailFlipbook` / `SetPixelFlipbook` 切换组件的 `Flipbook` 资产。

### 2.3 在 MCP 编辑器中给玩家蓝图（BP_PlayerCharacter）正确暴露组件

代码已经创建了：
- `DetailFlipbookComp` (UPaperFlipbookComponent)
- `PixelFlipbookComp` (UPaperFlipbookComponent)

你需要：
1. 打开 `Content/Characters/BP_PlayerCharacter`（或继承自 `APlayerCharacter` 的蓝图）。
2. 在 `Components` 面板应看到 `DetailFlipbookComp` 和 `PixelFlipbookComp`。
3. 调整它们的相对位置（`RelativeLocation` / `RelativeRotation`），让锚点对准角色脚底（默认 Z 偏移 -90）。
4. 设置 `bUsePixelArt = true`（默认即 true）以使 `PixelFlipbookComp` 可见。

> 世界生成完成后，`ABasePlayerController::OnWorldGenerationComplete` 会自动调用 `RequestPlayerCharacterImage`。生成完毕后会通过 `SetPixelFlipbook` 应用到玩家。

### 2.5 在 MCP 编辑器中给 BP_DialogueWidget 添加 NPCPortraitImage

代码声明了：
```cpp
UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
TObjectPtr<UImage> NPCPortraitImage;
```

你需要：
1. 打开 `Content/UI/Dialogue/BP_DialogueWidget`（继承自 `UDialogueWidget` 的蓝图）。
2. 在 Designer 面板中拖入一个 `Image` 控件，重命名为 `NPCPortraitImage`（**必须**与 `BindWidgetOptional` 指定的名称一致）。
3. 设置 `Brush` / `ZOrder` 使其显示在 NPC 名字上方或旁边。
4. 如不需要 UI 上的角色立绘，则**不添加**，代码会自动跳过（因 `BindWidgetOptional`）。

### 2.4 创建模板 Flipbook 资源（可选）

代码已支持运行时从 `UTexture2D` 动态构造单帧 `UPaperFlipbook`，无需预先制作。
但若希望在编辑器中提前创建可复用的 flipbook 模板（用于手工占位/测试）：

在 MCP 中调用工具（伪代码）：

```
MCP.execute("create_or_load_asset", {
    asset_name: "FB_NPC_Default",
    asset_class: "PaperFlipbook",
    save_path: "/Game/NPCs/Flipbooks"
});
```

将其默认 sprite 指向一个默认占位纹理（你可以重复使用 `T_Placeholder_NPC`）。
运行时若未生成图集，NPC 默认无可见图集组件（已设为 Collapsed）；这与现有默认占位 mesh 一起工作。

### 2.5 操作流程生成

整体运行流程（自上而下）：

1. **玩家进入游戏** → `UChunkManagerComponent::GenerateWorldFromPrompt` → LLM 生成初始场景 JSON。
2. **解析 JSON 中的 NPC** → `SpawnChunkAndNPCs` → `SpawnActor<ANPCCharacter>` → `SetNPCData`。
3. **触发 NPC 图集生成** → `ImageSubsystem->RequestCharacterImageForNPC(NPC, ...)`。
4. **世界生成完成** → `ABasePlayerController::OnWorldGenerationComplete` → `ImageSubsystem->RequestPlayerCharacterImage(PlayerCharacter)`。
5. **检查缓存**：
   - 命中内存缓存 → 立即应用 flipbook + UI portrait。
   - 命中磁盘缓存 → 加载本地 PNG 文件，作为运行时 flipbook 应用。
   - 未命中 → 启动 `UAICharacterImageSetAsyncAction`（火山引擎/ARK）异步生成 6 张图。
6. **回调 `HandleImageSetResponse`** → 按 CharacterId 找到对应角色 → `ApplyToNPCFlipbooks` / `ApplyToPlayerFlipbooks` + `OnCharacterImageReady.Broadcast`。
7. **玩家与 NPC 对话** → `StartDialogue` → 若缓存命中则 `ApplyCachedImageToDialogue` → `SetNPCPortrait`。
8. **第二次进入游戏** → 跳过步骤 4 的网络请求，直接从磁盘加载。

---

## 3. 常见错误与排查

| 现象 | 原因 | 解决 |
|------|------|------|
| `Failed to resolve provider config` | AIImageDeveloperSettings 未配置 | 在 Project Settings 中配置 Active Provider |
| NPC 没有显示 flipbook | `PixelFlipbookComp` 默认可见但无 flipbook 资产 | 等待首次异步请求；或手动放一个占位 flipbook |
| 玩家没有显示 flipbook | `PixelFlipbookComp` 默认可见但无 flipbook 资产 | 等待世界生成完成后的异步请求；或手动放一个占位 flipbook |
| DialogueWidget 不显示 portrait | 未在 BP_DialogueWidget 中放名为 `NPCPortraitImage` 的 Image 控件 | 添加同名 Image 控件或留空（将跳过） |
| 同一角色重复请求 | 进程崩溃前未清理 `PendingActions` | 启动时 `Initialize` 已清空，正常情况不会发生 |
| 动态委托匹配到错误角色 | 多角色同时生成完成 | 当前实现为「首个 pending action」匹配；如需并发需要改为每角色绑定独立回调或携带 Action 指针 |

---

## 4. 后续可扩展

- **多表情切换**：当前 `NPCCharacter` 默认显示 happy；可在 `OnImageReadyForCharacter` 中根据 LLM 返回的情绪标签切换 `FlipbookComponent` 的 `Sprite` 帧或切换不同 flipbook 资源。
- **玩家自定义描述**：目前 `BuildImageRequestForPlayer` 使用默认描述；可在 `UPluginGameInstance` 或 SaveGame 中保存玩家自定义外观，然后写入 `FAICharacterSpec` 的 `Personality` / `Outfit` 等字段。
- **场景图**：复用 `AIImagePaths::GetSceneCacheDirectory` 在 Chunk 加载完成后异步生成场景背景图，并应用到 `AGameChunkActor` 的 `ChunkMesh` 材质。
