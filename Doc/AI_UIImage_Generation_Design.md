# AI 生成UI图片资源 设计文档

## 1. 需求概述

根据剧情风格（如武侠、赛博朋克、中世纪奇幻等），由AI自动生成对应的UI背景图、按钮纹理、图标等资源，并动态应用到游戏UI上，实现沉浸式剧情体验。

## 2. 核心功能

### 2.1 风格映射系统

| 剧情风格 | UI风格描述 | 主色调 | 参考关键词 |
|---------|-----------|--------|-----------|
| 武侠古风 | 水墨山水、宣纸纹理、毛笔字体 | 米白、朱红、墨黑 | chinese ink wash painting, rice paper texture, wuxia style |
| 赛博朋克 | 霓虹灯光、故障艺术、科技感 | 霓虹粉、电光蓝、深紫 | cyberpunk, neon lights, glitch effect, futuristic UI |
| 中世纪奇幻 | 羊皮纸、金属边框、魔法光效 | 金色、深棕、幽蓝 | medieval fantasy, parchment, ornate border, magical glow |
| 末日废土 | 铁锈、破损、工业风 | 铁锈红、土黄、深灰 | post-apocalyptic, rusty metal, worn texture, industrial |
| 现代都市 | 简约、玻璃质感、扁平化 | 白色、天蓝、深灰 | modern minimalist, glass morphism, clean UI |

### 2.2 生成内容类型

1. **对话框背景** - 带边框的对话面板背景图
2. **按钮三态纹理** - 正常/悬停/按下状态的按钮背景
3. **主界面背景** - 整体UI背景/边框装饰
4. **装饰元素** - 边角装饰、分隔线、图标
5. **特殊特效图** - 魔法光效、血迹、水墨晕染等

### 2.3 输出规格

| 资源类型 | 尺寸 | 格式 | 备注 |
|---------|------|------|------|
| 对话框背景 | 1024x512 | PNG (带透明通道) | 可九宫格拉伸 |
| 按钮背景 | 256x128 | PNG (带透明通道) | 可九宫格拉伸 |
| 主界面背景 | 1920x1080 | PNG | 全屏背景 |
| 装饰元素 | 128x128 / 256x256 | PNG (带透明通道) | 小图标/装饰 |
| 特效图 | 512x512 | PNG (带透明通道) | 粒子/光效 |

## 3. 技术架构

### 3.1 整体架构

```
┌─────────────────────────────────────────────────────┐
│                   UI Manager (C++)                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐ │
│  │ Style Preset│  │ Image Gen   │  │ Cache Mgr   │ │
│  │  (风格预设)  │  │   Client    │  │  (缓存管理) │ │
│  └─────────────┘  └─────────────┘  └─────────────┘ │
│         │                │                │         │
│         ▼                ▼                ▼         │
│  ┌───────────────────────────────────────────────┐  │
│  │            UUIImageGenerator Subsystem        │  │
│  └───────────────────────────────────────────────┘  │
└──────────────────────────────┬──────────────────────┘
                               │
                               ▼
                     ┌───────────────────┐
                     │ AI Image API      │
                     │ (DALL-E / SD /...)│
                     └───────────────────┘
```

### 3.2 核心模块

#### 3.2.1 风格预设管理 (FUIStylePreset)

```cpp
USTRUCT(BlueprintType)
struct FUIStylePreset
{
    GENERATED_BODY()
    
    // 风格名称
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StyleName;
    
    // AI生成Prompt模板
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BackgroundPromptTemplate;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ButtonPromptTemplate;
    
    // 主色调（用于文字/边框颜色）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor PrimaryColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SecondaryColor;
    
    // 文字颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TextColor;
    
    // 按钮颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ButtonNormalColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ButtonHoverColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ButtonPressedColor;
};
```

#### 3.2.2 AI图片生成器 (UUIImageGeneratorSubsystem)

```cpp
UCLASS()
class UUIImageGeneratorSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
    
public:
    // 生成背景图
    UFUNCTION(BlueprintCallable)
    void GenerateBackground(const FString& StylePrompt, 
                           FOnImageGenerated OnComplete,
                           FOnImageGenerationFailed OnFailed);
    
    // 生成按钮纹理
    UFUNCTION(BlueprintCallable)
    void GenerateButtonTexture(const FString& StylePrompt,
                               FOnImageGenerated OnComplete,
                               FOnImageGenerationFailed OnFailed);
    
    // 应用风格到指定Widget
    UFUNCTION(BlueprintCallable)
    void ApplyStyleToWidget(UUserWidget* TargetWidget, 
                           const FUIStylePreset& StylePreset);
    
    // 从缓存获取纹理
    UFUNCTION(BlueprintCallable)
    UTexture2D* GetCachedTexture(const FString& TextureKey);
    
private:
    // HTTP请求处理
    void SendImageGenerationRequest(const FString& Prompt,
                                    const FString& TextureKey,
                                    FOnImageGenerated OnComplete,
                                    FOnImageGenerationFailed OnFailed);
    
    // 纹理加载
    UTexture2D* LoadTextureFromData(const TArray<uint8>& ImageData);
    
    // 缓存管理
    UPROPERTY()
    TMap<FString, TWeakObjectPtr<UTexture2D>> TextureCache;
    
    // 磁盘缓存路径
    FString GetDiskCachePath() const;
};
```

#### 3.2.3 缓存策略

- **内存缓存**：运行时使用LRU策略，最多缓存N张纹理
- **磁盘缓存**：将生成的图片保存到本地，下次启动直接加载
- **缓存Key**：由风格名称 + 资源类型 + 版本号组成

## 4. API接口设计

### 4.1 支持的AI图片API

| API名称 | 优点 | 缺点 | 推荐度 |
|---------|------|------|--------|
| DALL-E 3 | 质量高，风格多样 | 成本高，生成慢 | ⭐⭐⭐ |
| Stable Diffusion (本地) | 免费，可控性强 | 需要显卡，部署复杂 | ⭐⭐⭐⭐ |
| Midjourney | 艺术风格强 | 无API，需自动化 | ⭐⭐ |
| 国内API (通义万相/文心一格等) | 速度快，中文理解好 | 风格选择较少 | ⭐⭐⭐⭐ |

### 4.2 HTTP请求格式

```json
{
  "prompt": "cyberpunk style dialogue box background, neon blue border, dark background, transparent edges, UI design, high quality",
  "negative_prompt": "text, watermark, blurry, low quality",
  "size": "1024x512",
  "format": "png"
}
```

## 5. 蓝图集成方案

### 5.1 调用时机

```
剧情切换事件
    ↓
判断当前风格是否已生成
    ├─ 已生成 → 从缓存加载，直接应用
    └─ 未生成 → 显示Loading → 调用AI生成 → 保存缓存 → 应用到UI
```

### 5.2 蓝图节点

- `Generate UIImage By Style` - 根据风格名称生成全套UI资源
- `Apply Style To Widget` - 将风格预设应用到目标Widget
- `Get Cached Texture` - 从缓存获取纹理
- `Clear Image Cache` - 清除缓存

## 6. 性能与优化

### 6.1 预生成策略

- 游戏启动时预生成当前章节的UI资源
- 过场动画时后台预生成下一场景的资源
- 提供"下载全部风格资源"选项

### 6.2 降级方案

- 如果AI生成失败，使用默认的程序化生成的风格
- 支持手动替换AI生成的图片为美术资源
- 提供风格预设库，玩家可以选择喜欢的风格

### 6.3 内存管理

- 纹理使用 `Streaming` 模式，按需加载
- 切换场景时自动卸载不使用的纹理
- 低内存设备上自动降低纹理分辨率

## 7. 实现路线图

### Phase 1: 基础框架 (1-2天)
- [ ] 创建 UUIImageGeneratorSubsystem
- [ ] 实现 FUIStylePreset 结构体
- [ ] 实现基础的HTTP请求功能
- [ ] 实现纹理加载和内存缓存

### Phase 2: 风格系统 (2-3天)
- [ ] 实现风格预设配置表（DataAsset）
- [ ] 实现Prompt模板系统
- [ ] 集成至少一个AI图片API
- [ ] 实现磁盘缓存功能

### Phase 3: UI集成 (2天)
- [ ] 实现ApplyStyleToWidget功能
- [ ] 支持动态切换DialogueWidget风格
- [ ] 支持动态切换WorldSetupWidget风格
- [ ] 实现风格切换动画过渡

### Phase 4: 优化与扩展 (2-3天)
- [ ] 添加更多风格预设
- [ ] 实现预生成和后台生成
- [ ] 添加失败重试和降级方案
- [ ] 性能优化和内存管理

## 8. 待确认问题

1. 使用哪个AI图片API？（预算、风格偏好）
2. 是否需要本地部署Stable Diffusion？
3. UI风格切换是否需要动画过渡？
4. 是否支持玩家自定义风格？
5. 资源缓存的大小上限是多少？
