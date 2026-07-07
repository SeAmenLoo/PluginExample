# ⚠️ 已废弃 - AI 生成UI图片资源设计文档

> **注意**：本文档描述的设计已废弃，功能已由 [AIImageGenerator](file:///e:/Projects/UE/VRGame/git/PluginExample/Plugins/AIImageGenerator/) 插件实现。

## 替代方案

请参考以下文档获取最新信息：

| 文档 | 说明 |
|------|------|
| [AIImageGenerator 编辑器指南](file:///e:/Projects/UE/VRGame/git/PluginExample/Plugins/AIImageGenerator/Doc/EditorGuide.md) | AIImageGenerator 插件使用指南 |
| [角色图生成集成流程](file:///e:/Projects/UE/VRGame/git/PluginExample/Doc/CharacterImage_Integration_Workflow.md) | 角色图生成与游戏对话流程集成 |
| [角色资产生成工作流](file:///e:/Projects/UE/VRGame/git/PluginExample/Doc/Character_Asset_Generation_Workflow.md) | 角色资产（细节图+像素图）生成工作流设计 |
| [UI_AI_Styling_Guide.md](file:///e:/Projects/UE/VRGame/git/PluginExample/Doc/UI_AI_Styling_Guide.md) | UI美化方案指南（包含主题模板） |

---

## 原始文档摘要（仅供参考）

### 原设计目标

根据剧情风格（如武侠、赛博朋克、中世纪奇幻等），由AI自动生成对应的UI背景图、按钮纹理、图标等资源，并动态应用到游戏UI上，实现沉浸式剧情体验。

### 原设计架构

- **风格映射系统**：将剧情风格映射到UI风格描述、主色调、参考关键词
- **生成内容类型**：对话框背景、按钮三态纹理、主界面背景、装饰元素、特殊特效图
- **Provider模式**：支持多种AI图片API（火山方舟、OpenAI兼容、通用HTTP、本地命令）

### 当前实现状态

所有功能已在 AIImageGenerator 插件中实现，包括：
- ✅ 角色图生成（细节锚点图、像素锚点图、表情图、动作图）
- ✅ 场景资源图生成
- ✅ UI素材生成
- ✅ Provider配置系统
- ✅ 缓存机制
- ✅ 异步生成节点