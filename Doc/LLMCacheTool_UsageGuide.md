# LLM 缓存工具操作文档

## 概述

本地缓存AI生成内容工具，用于测试游戏流程时节省Token。支持：
- 自动缓存AI响应（世界生成、分块生成、对话）
- 多录制文件管理（每次游戏生成独立录制文件）
- 缓存重放模式（完全离线使用缓存，零Token消耗）
- 缓存导入导出（在不同机器/项目间共享）
- 蓝图可视化控制

---

## 快速开始

### 1. 录制模式（默认）

每次启动游戏会自动创建一个新的录制文件：

```
PluginExample/Saved/CachedLLM/recording_20260630_143025.json
```

文件名格式：`recording_YYYYMMDD_HHMMSS.json`

游戏过程中所有AI响应会自动保存到当前录制文件中。

### 2. 重放模式（零Token）

1. 在蓝图中调用 `GetAllRecordings` 获取所有录制文件列表
2. 调用 `LoadRecording(FileName)` 加载指定录制
3. 设置 `bUseCacheReplay = true` 或调用 `SetReplayMode(true)`
4. 运行游戏，直接使用缓存的响应

### 3. ChunkManagerComponent 配置

在 `ChunkManagerComponent` 的 Details 面板中：

| 属性 | 分类 | 默认值 | 说明 |
|------|------|--------|------|
| `bEnableCache` | LLM\|Cache | true | 启用缓存功能 |
| `bUseCacheReplay` | LLM\|Cache | false | 开启后使用缓存响应，不调用真实API |

---

## 录制文件管理

### 录制文件位置

```
工程目录/Saved/CachedLLM/
    ├── recording_20260630_143025.json
    ├── recording_20260630_152010.json
    └── recording_20260630_164530_MyTest.json
```

### 录制文件命名规则

| 格式 | 示例 | 说明 |
|------|------|------|
| 默认 | `recording_20260630_143025.json` | 时间戳自动生成 |
| 自定义名称 | `recording_20260630_143025_TestScene.json` | 附加自定义名称 |

### FLLMRecordingInfo 结构

| 字段 | 类型 | 说明 |
|------|------|------|
| `FileName` | FString | 文件名 |
| `FilePath` | FString | 完整路径 |
| `CreatedAt` | FDateTime | 创建时间 |
| `EntryCount` | int32 | 缓存条目数量 |
| `FileSize` | int64 | 文件大小（字节） |

---

## 蓝图控制

### 获取缓存子系统

```
Get Game Instance → Get Subsystem (Class = LLMCacheSubsystem)
```

### 录制管理接口

| 函数名 | 分类 | 功能 | 参数 | 返回值 |
|--------|------|------|------|--------|
| `StartNewRecording` | LLM\|Recording | 开始新录制 | CustomName (可选) | - |
| `GetAllRecordings` | LLM\|Recording | 获取所有录制列表 | - | TArray\<FLLMRecordingInfo\> |
| `LoadRecording` | LLM\|Recording | 加载指定录制 | FileName | bool |
| `DeleteRecording` | LLM\|Recording | 删除指定录制 | FileName | bool |
| `GetCurrentRecordingName` | LLM\|Recording | 获取当前录制名 | - | FString |

### 缓存控制接口

| 函数名 | 分类 | 功能 | 参数 | 返回值 |
|--------|------|------|------|--------|
| `SetReplayMode` | LLM\|Cache | 设置重放模式 | bool bEnabled | - |
| `IsReplayModeEnabled` | LLM\|Cache | 获取重放状态 | - | bool |
| `SetCacheWritingEnabled` | LLM\|Cache | 设置缓存写入 | bool bEnabled | - |
| `IsCacheWritingEnabled` | LLM\|Cache | 获取写入状态 | - | bool |
| `GetCacheEntryCount` | LLM\|Cache | 获取缓存条目数 | - | int32 |
| `ClearCache` | LLM\|Cache | 清空当前缓存 | - | - |
| `ExportCache` | LLM\|Cache | 导出缓存到目录 | DirectoryPath | bool |
| `ImportCache` | LLM\|Cache | 从目录导入缓存 | DirectoryPath | bool |
| `GetAllCacheEntries` | LLM\|Cache | 获取所有缓存条目 | - | TArray\<FLLMCacheEntry\> |
| `FindCacheEntryByContext` | LLM\|Cache | 按上下文查找 | ContextSubstring, Out Entries | bool |
| `GetLastWorldGenCacheEntry` | LLM\|Cache | 获取最新世界生成缓存 | Out Entry | bool |

---

## 使用场景

### 场景一：开发调试（录制 + 重放）

1. **首次运行（录制）**：
   - 游戏自动创建 `recording_XXX.json`
   - 走完完整游戏流程（生成世界 → 进入Chunk → 与NPC对话）
   - 所有AI响应自动保存

2. **后续调试（重放）**：
   - 调用 `LoadRecording("recording_XXX.json")`
   - 调用 `SetReplayMode(true)`
   - 再次运行相同流程，完全不消耗Token

### 场景二：多场景测试

1. 为每个测试场景录制独立文件：
   ```cpp
   // 录制"新手村"场景
   CacheSubsystem->StartNewRecording(TEXT("新手村场景"));
   // ... 运行测试 ...

   // 录制"地下城"场景
   CacheSubsystem->StartNewRecording(TEXT("地下城场景"));
   // ... 运行测试 ...
   ```

2. 测试时加载对应录制文件即可

### 场景三：团队共享缓存

1. 一人录制完整流程
2. 将 `Saved/CachedLLM/` 目录下的文件分享给团队
3. 其他人放入对应目录，用 `LoadRecording` 加载

### 场景四：回归测试

1. 为每个版本录制一套基准缓存
2. 新版本测试时加载旧版本录制
3. 快速验证功能是否正常

---

## 缓存条目结构 (`FLLMCacheEntry`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `RequestHash` | FString | 请求内容的哈希值（缓存键） |
| `RequestMessages` | TArray\<FLLMChatMessage\> | 原始请求消息 |
| `ResponseContent` | FString | AI返回的内容 |
| `ResponseJson` | FString | 解析后的JSON |
| `bWasSuccessful` | bool | 请求是否成功 |
| `CachedAt` | FDateTime | 缓存时间 |
| `RequestType` | FString | 请求类型 |
| `ContextDescription` | FString | 上下文描述 |

---

## 缓存的请求类型

| 类型标识 | 触发场景 | 说明 |
|----------|----------|------|
| `WorldGen` | 世界生成 | GenerateWorldFromPrompt() |
| `ChunkGen` | 分块生成 | GenerateNextChunk() |
| `Dialogue` | NPC对话 | SendDialogueRequest() |

---

## 注意事项

1. **每次游戏新录制**：启动游戏会自动创建新的录制文件，不会覆盖旧文件
2. **缓存匹配规则**：基于请求消息内容的哈希值，内容完全相同才会命中缓存
3. **对话缓存**：由于对话历史不断累加，每条对话都会产生独立缓存
4. **文件数量**：录制文件会持续增长，定期使用 `DeleteRecording` 清理
5. **重放时不写入**：重放模式下默认不写入新缓存（可通过 `SetCacheWritingEnabled` 修改）

---

## C++ 使用示例

```cpp
// 获取缓存子系统
ULLMCacheSubsystem* CacheSubsystem = GetGameInstance()->GetSubsystem<ULLMCacheSubsystem>();
if (!CacheSubsystem) return;

// 查看所有录制文件
TArray<FLLMRecordingInfo> Recordings = CacheSubsystem->GetAllRecordings();
for (const FLLMRecordingInfo& Info : Recordings)
{
    UE_LOG(LogTemp, Log, TEXT("录制: %s, 条目数: %d"), *Info.FileName, Info.EntryCount);
}

// 加载最新录制并重放
if (Recordings.Num() > 0)
{
    CacheSubsystem->LoadRecording(Recordings[0].FileName);
    CacheSubsystem->SetReplayMode(true);
}

// 开始新的命名录制
CacheSubsystem->StartNewRecording(TEXT("Boss战测试"));

// 删除旧录制
CacheSubsystem->DeleteRecording(TEXT("recording_20260630_143025.json"));
```

---

## 相关文件

| 文件 | 说明 |
|------|------|
| [LLMCacheSubsystem.h](file:///e:/Projects/UE/VRGame/git/PluginExample/Plugins/LLMRuntime/Source/LLMRuntime/Public/LLMCacheSubsystem.h) | 缓存子系统头文件 |
| [LLMCacheSubsystem.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Plugins/LLMRuntime/Source/LLMRuntime/Private/LLMCacheSubsystem.cpp) | 缓存子系统实现 |
| [ChunkManagerComponent.h](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Public/ChunkManagerComponent.h) | Chunk管理器（缓存集成） |
| [ChunkManagerComponent.cpp](file:///e:/Projects/UE/VRGame/git/PluginExample/Source/PluginExample/Private/ChunkManagerComponent.cpp) | Chunk管理器实现 |
