# UE C++ 开发规则 (用于AI代码生成)

## 1. 枚举类型使用规则

### 1.1 TextRenderComponent 垂直对齐
```cpp
// 正确 (UE5)
ChunkNameLabel->SetVerticalAlignment(EVRTA_TextCenter);

// 错误 - 不要使用
// EVRTA_Center (UE4中存在，UE5已移除)
// EVTextVerticalAlignment::EVRTA_Center
```

### 1.2 TextRenderComponent 水平对齐
```cpp
// 正确
ChunkNameLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
```

### 1.3 碰撞响应
```cpp
// 正确
Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
```

## 2. 委托/回调规则

### 2.1 动态多播委托 (DECLARE_DYNAMIC_MULTICAST_DELEGATE)
```cpp
// 绑定时使用 AddDynamic，不使用 AddLambda
Action->Completed.AddDynamic(this, &UMyClass::HandleResponse);

// 回调函数必须标记 UFUNCTION()
UFUNCTION()
void HandleResponse(const FLLMChatResponse& Response);
```

### 2.2 调用多播委托
```cpp
// 正确 - 使用 Broadcast()
OnWorldSetupComplete.Broadcast(Prompt);

// 错误 - 不要使用 Execute()
// Execute() 用于单播委托
```

### 2.3 动态委托不支持的函数
- **禁止使用**: `AddLambda`, `AddUObject`, `AddSP`
- **只能使用**: `AddDynamic`, `AddUniqueDynamic`

## 3. JSON序列化/反序列化规则

### 3.1 JsonReader 创建
```cpp
// 正确 - 使用 TSharedRef
TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

// 错误 - 不要使用 TSharedPtr
// TSharedPtr<TJsonReader<>> Reader = ...
```

### 3.2 Json反序列化
```cpp
// 正确
TSharedPtr<FJsonObject> JsonObject;
if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
{
    // 使用 JsonObject
}

// JsonObject 直接获取值
FString Name = JsonObject->GetStringField(TEXT("name"));
```

## 4. 蓝图类加载规则

### 4.1 加载UserWidget类
```cpp
// 正确 - 使用 LoadClass
UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("WidgetBlueprint'/Game/UI/WB_WorldSetup.WB_WorldSetup_C'"));
if (WidgetClass)
{
    auto Widget = CreateWidget<UUserWidget>(this, WidgetClass);
}
```

### 4.2 软引用路径格式
```cpp
// 格式: Blueprint'/Game/Path.To.Asset.Asset_C'
FSoftObjectPath Path(TEXT("Blueprint'/Game/UI/MyWidget.MyWidget_C'"));
```

## 5. 字符串处理规则

### 5.1 禁止使用原始字符串字面量
```cpp
// 禁止 - LR"()" 在UE中与TEXT宏混用会有问题
// return LR"(...)";

// 正确 - 使用 TEXT() 拼接
FString Result = TEXT("Hello ");
Result += WorldName;
Result += TEXT("\n");
```

### 5.2 TEXT() 宏使用
```cpp
// 正确
FString Name = TEXT("Player");
FString Path = TEXT("Blueprint'/Game/UI/Widget.Widget_C'");

// 字符串插值使用 FString::Printf
FString Info = FString::Printf(TEXT("Player: %s, Level: %d"), *PlayerName, Level);
```

## 6. UPROPERTY规则

### 6.1 支持反射的类型
```cpp
// 正确
UPROPERTY()
TArray<FString> Names;

UPROPERTY()
FString PlayerName;

UPROPERTY()
int32 Score;
```

### 6.2 不支持反射的类型 (不能直接用作UPROPERTY)
- `TSharedPtr`, `TSharedRef`
- `TMap<FString, TSharedPtr<...>>`
- `TPair` (需要用结构体包装)

## 7. TArray遍历规则

```cpp
// 正确 - 使用范围for
for (const FString& Item : Items)
{
    UE_LOG(LogTemp, Log, TEXT("Item: %s"), *Item);
}

// 正确 - 使用索引
for (int32 i = 0; i < Items.Num(); ++i)
{
    UE_LOG(LogTemp, Log, TEXT("Item %d: %s"), i, *Items[i]);
}
```

## 8. 类引用与对象引用

### 8.1 TSubclassOf 使用
```cpp
// 正确 - 用于指定类类型
UPROPERTY(EditAnywhere, BlueprintReadWrite)
TSubclassOf<AGameChunkActor> ChunkActorClass;
```

### 8.2 TWeakObjectPtr 使用
```cpp
// 正确 - 用于可能失效的引用
TWeakObjectPtr<APawn> CurrentPlayer;
```

## 9. 游戏框架组件

### 9.1 SpawnActor
```cpp
FActorSpawnParameters SpawnParams;
SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

AGameChunkActor* Actor = GetWorld()->SpawnActor<AGameChunkActor>(ChunkClass, Location, Rotation, SpawnParams);
```

### 9.2 GameInstanceSubsystem
```cpp
void UMySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // 初始化代码
}

void UMySubsystem::Deinitialize()
{
    // 清理代码
    Super::Deinitialize();
}
```

## 10. 常见错误对照表

| 错误类型 | 错误写法 | 正确写法 |
|---------|---------|---------|
| 垂直对齐 | `EVRTA_Center` | `EVRTA_TextCenter` |
| 多播委托调用 | `Delegate.Execute()` | `Delegate.Broadcast()` |
| JsonReader | `TSharedPtr<TJsonReader<>>` | `TSharedRef<TJsonReader<>>` |
| 委托绑定 | `AddLambda` | `AddDynamic` |
| 类加载 | `Path.TryLoadClass<>()` | `LoadClass<>()` |
| 原始字符串 | `LR"(...)"` | `TEXT("...")` |

## 11. AI生成代码检查清单

生成代码后，检查以下项目：
- [ ] 枚举值是否存在于UE5中
- [ ] 委托绑定是否使用 AddDynamic
- [ ] 多播委托是否使用 Broadcast() 调用
- [ ] Json操作是否使用 TSharedRef
- [ ] 是否避免了 LR"()" 原始字符串
- [ ] UPROPERTY类型是否支持反射

## 12. 参考项目

项目中正确实现的示例代码：
- `ChunkManagerComponent.cpp` - JSON解析、委托回调
- `GameChunkActor.cpp` - TextRenderComponent设置
- `WorldSetupWidget.cpp` - 多播委托使用
- `PluginGameInstance.cpp` - 蓝图类加载
