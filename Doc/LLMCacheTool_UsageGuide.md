# LLM 缓存工具操作文档

## 概述

本地缓存AI生成内容工具，用于测试游戏流程时节省Token。支持：
- 自动缓存AI响应（世界生成、分块生成、对话）
- 缓存重放模式（完全离线使用缓存，零Token消耗）
- 缓存导入导出（在不同机器/项目间共享）
- 蓝图可视化控制

---

## 快速开始

### 1. 开启缓存（默认已开启）

在 `ChunkManagerComponent` 的 Details 面板中：

| 属性 | 分类 | 默认值 | 说明 |
|------|------|--------|------|
| `bEnableCache` | LLM\|Cache | true | 启用缓存功能 |
| `bUseCacheReplay` | LLM\|Cache | false | 开启后使用缓存响应，不调用真实API |

### 2. 首次运行（录制模式）

1. 确保 `bEnableCache = true`，`bUseCacheReplay = false`
2. 正常运行游戏，完成你想测试的流程
3. 所有AI响应会自动缓存到本地文件

### 3. 重放模式（零Token）

1. 首次运行完成后，设置 `bUseCacheReplay = true`
2. 再次运行相同流程，将直接使用缓存的响应
3. 完全不消耗Token，响应速度极快

---

## 蓝图控制

### 获取缓存子系统

```
Get Game Instance → Get Subsystem (Class = LLMCacheSubsystem)
    → Set Replay Mode (True)
```

### 常用蓝图接口

| 函数名 | 功能 | 参数 | 返回值 |
|--------|------|------|--------|
| `SetReplayMode` | 设置重放模式 | bool bEnabled | - |
| `IsReplayModeEnabled` | 获取重放状态 | - | bool |
| `SetCacheWritingEnabled` | 设置是否写入缓存 | bool bEnabled | - |
| `IsCacheWritingEnabled` | 获取写入状态 | - | bool |
| `GetCacheEntryCount` | 获取缓存条目数 | - | int32 |
| `ClearCache` | 清空所有缓存 | - | - |
| `ExportCache` | 导出缓存到目录 | FString DirectoryPath | bool (是否成功) |
| `ImportCache` | 从目录导入缓存 | FString DirectoryPath | bool (是否成功) |
| `GetAllCacheEntries` | 获取所有缓存条目 | - | TArray\<FLLMCacheEntry\> |
| `FindCacheEntryByContext` | 按上下文查找缓存 | FString ContextSubstring, Out Entries | bool |

---

## 缓存位置

### 默认缓存文件

```
工程目录/Saved/CachedLLM/response_cache.json
```

例如：
- 工程路径: `E:\Projects\UE\VRGame\git\PluginExample\Saved\CachedLLM\response_cache.json`

### 缓存条目结构 (`FLLMCacheEntry`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `RequestHash` | FString | 请求内容的哈希值（缓存键） |
| `RequestMessages` | TArray\<FLLMChatMessage\> | 原始请求消息 |
| `ResponseContent` | FString | AI返回的内容 |
| `ResponseJson` | FString | 解析后的JSON |
| `bWasSuccessful` | bool | 请求是否成功 |
| `CachedAt` | FDateTime | 缓存时间 |
| `RequestType` | FString | 请求类型 (WorldGen/ChunkGen/Dialogue) |
| `ContextDescription` | FString | 上下文描述 |

---

## 缓存的请求类型

| 类型标识 | 触发场景 | 说明 |
|----------|----------|------|
| `WorldGen` | 世界生成 | GenerateWorldFromPrompt() |
| `ChunkGen` | 分块生成 | GenerateNextChunk() |
| `Dialogue` | NPC对话 | SendDialogueRequest() |

---

## 使用场景

### 场景一：开发调试

1. 首次运行，走完完整游戏流程（生成世界 → 进入Chunk → 与NPC对话）
2. 打开重放模式
3. 后续调试UI/逻辑时，完全不消耗Token

### 场景二：团队共享缓存

1. 一人录制完整流程
2. 使用 `ExportCache` 导出缓存文件
3. 其他人使用 `ImportCache` 导入
4. 全队共享，节省Token

### 场景三：回归测试

1. 为每个测试场景录制一套缓存
2. 测试时切换到重放模式
3. 快速验证功能是否正常

---

## 注意事项

1. **缓存匹配规则**：基于请求消息内容的哈希值，内容完全相同才会命中缓存
2. **对话缓存**：由于对话历史不断累加，每条对话都会产生独立缓存
3. **温度参数**：相同prompt不同Temperature会产生不同缓存（但Temperature在Request.Messages外，不影响哈希）
4. **缓存清理**：缓存文件会持续增长，定期使用 `ClearCache` 清理

---

## C++ 使用示例

```cpp
// 获取缓存子系统
ULLMCacheSubsystem* CacheSubsystem = GetGameInstance()->GetSubsystem<ULLMCacheSubsystem>();
if (!CacheSubsystem) return;

// 开启重放模式
CacheSubsystem->SetReplayMode(true);

// 导出缓存
CacheSubsystem->ExportCache(TEXT("D:/MyGameCache"));

// 导入缓存
CacheSubsystem->ImportCache(TEXT("D:/MyGameCache"));

// 查看缓存数量
int32 Count = CacheSubsystem->GetCacheEntryCount();
UE_LOG(LogTemp, Log, TEXT("缓存条目数: %d"), Count);

// 清空缓存
CacheSubsystem->ClearCache();
```

---

## 相关文件

| 文件 | 说明 |
|------|------|
| [LLMCacheSubsystem.h](file:///e:/Projects/UE/VRGame/git/PluginExample/Plugins/LLMRuntime/Source/LLMRuntime/Public/LLMCacheSubsystem.h) | 缓存子系统头文件 |
| [LLMCacheSubsystem.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Plugins/LLMRuntime/Source/LLMRuntime/Private/LLMCacheSubsystem.cpp) | 缓存子系统实现 |
| [ChunkManagerComponent.h](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Public/ChunkManagerComponent.h) | Chunk管理器（缓存集成） |
| [ChunkManagerComponent.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Private/ChunkManagerComponent.cpp) | Chunk管理器实现 |
