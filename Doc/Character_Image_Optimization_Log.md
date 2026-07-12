# 角色图生成优化记录

## 1. 本轮目标

围绕角色图生成链路，解决以下问题：

1. 像素图生成失败后流程卡住
2. 生成顺序与目标产线不一致
3. 细节图视角、裁切和比例不稳定
4. 细节图与参考图相似度过高
5. 细节图与像素图朝向不统一

目标顺序固定为：

1. 角色细节图
2. 角色像素图
3. 像素行走差分
4. 角色表情图

## 2. 问题复盘

### 2.1 日志与缓存暴露出的核心问题

- `Doc/Error.txt` 中最后失败的是 `PixelAnchor` 的 HTTP 请求，说明像素锚点是当前最脆弱环节
- `Saved/CharacterImages/Player` 当前只有 `detail` 产物，说明像素链路中断后没有正常收尾
- 旧缓存中的 `Saved/CharacterImages/cache/old/npc_0/pixel/npc_0_pixel_anchor_01.jpg` 生成成了多视图设定板，说明问题不只是网络失败，也包括提示词和参考图约束失焦

### 2.2 代码层根因

- 原流程为 `DetailAnchor -> PixelAnchor + Expressions 并行 -> WalkLeft/WalkRight`
- `DetailAnchor` 或 `PixelAnchor` 失败时，没有把依赖步骤补成失败完成态，可能导致异步动作永远不结束
- `HandleImageSetResponse()` 原先按第一个 pending action 反查角色，并发时可能把响应套到错误角色
- 细节图默认规则包含“半身或全身”，与提示词中的“必须半身”冲突
- 像素参考图优先使用 `Res/D_Pixel.png`，这张参考图是多视图设定板，不利于统一朝向和单视图输出
- 细节锚点会直接带入 `Res/Detail.png` 作为参考图，容易把角色往参考图外形上拉

## 3. 本轮已完成优化

### 3.1 流程时序

已将生成顺序调整为严格串行：

1. `DetailAnchor`
2. `PixelAnchor`
3. `WalkLeft`
4. `WalkRight`
5. `Happy`
6. `Angry`
7. `Sad`

这样可以降低并发请求对像素步骤的干扰，并与目标产线保持一致。

### 3.2 失败收口

已补充失败闭环：

- `DetailAnchor` 失败时，后续所有依赖步骤会被标记为 skipped/failed，并正常结束总流程
- `PixelAnchor` 失败时，行走差分与表情步骤会被标记为 skipped/failed，并正常结束总流程
- 总完成条件不再简单依赖计数，而是区分全成功与部分失败

### 3.3 角色绑定

已在 `FAICharacterGenerationResponse` 中加入 `CharacterId`，使回调可以优先按角色 ID 匹配，降低并发错绑风险。

### 3.4 提示词约束

已强化以下约束：

- 细节图固定为头部朝右的半身近景立绘
- 像素图固定为整体朝右的三头身单人全身单视图
- 明确禁止多视角、角色设定板、sprite sheet、拼贴、多角色
- 表情图只允许改表情，不允许改构图、朝向、裁切和服装
- 行走差分只允许改腿部迈步和轻微摆臂，不允许改朝向和视角

### 3.5 参考图策略

已做两项降风险处理：

- 细节锚点不再直接注入 `Res/Detail.png` 作为参考图
- 像素风参考图改为优先使用 `Res/Pixel.png`，仅在缺失时回退到 `Res/D_Pixel.png`

## 4. 当前进度

- [x] 梳理日志、缓存与现有链路
- [x] 安装 `skills.volces.com/byted-seedream-image-generate`
- [x] 调整生成顺序为串行
- [x] 修复失败步骤无法收尾的问题
- [x] 收紧细节图、像素图、表情图、行走图提示词
- [x] 优化角色响应绑定方式
- [x] 优化参考图使用策略
- [x] 开始处理主角像素优先与分阶段应用
- [x] 诊断验证（当前改动文件静态诊断通过）
- [ ] 运行一次真实生成验证新链路

## 4.1 本轮新增调整

- 在 `UAICharacterImageSetAsyncAction` 中加入进度广播入口，准备支持 `DetailAnchor` 成功后先更新 UI、`PixelAnchor` 成功后先更新场景角色
- 在 `UCharacterImageSubsystem` 中新增 `HandleImageSetProgress()`，允许在总流程完成前先把已成功的细节图/像素图应用到角色与 UI
- 在异步动作层加入“主角优先释放 NPC 队列”的调度雏形，目标是先让 `Player` 跑到 `PixelAnchor`
- 在异步动作回调侧加入步骤去重，避免同一步骤被晚到的失败回调二次污染

## 5. 建议的下一步验证

1. 清理一个测试角色的旧缓存后重新生成，确认输出顺序是否为“细节 -> 像素 -> 行走 -> 表情”
2. 重点检查像素锚点是否仍出现多视图设定板
3. 观察细节图是否稳定为右向半身近景
4. 对比细节图与旧 `Res/Detail.png` 的相似度，确认是否明显下降
5. 验证 `PixelAnchor` 失败时流程是否能正常结束并广播失败，而不是一直卡在生成中

## 6. 仍需关注的风险

- `AIImageService.cpp` 里的重试仍使用同步 `Sleep`，高并发时可能拖慢请求线程
- 当前仍依赖外部图像服务稳定性，HTTP 失败不会因为提示词优化而完全消失
- 如果后续仍出现角色设定板输出，建议直接替换或删除 `Res/D_Pixel.png`
- 如果需要更强的一致性，建议后续引入更明确的角色圣经或多阶段验收机制
