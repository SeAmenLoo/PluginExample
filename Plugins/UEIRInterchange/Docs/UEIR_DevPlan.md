# UEIR Interchange 开发计划（草案）

本文用于跟踪 UEIRInterchange 插件与配套 Skill 的开发进度、目标边界与主要瓶颈，便于后续持续推进。

支持级别定义：

- ✅ 支持：端到端可用（含稳定输入输出与报告）
- 🟡 部分：仅导出/仅子集/不保证可逆/仅少量类型
- 🧪 草案：接口/数据结构预留但尚未形成可用工作流
- ❌ 不在当前阶段：明确不做或优先级很低

## 方向修正（重要）

1) 编辑器资产转换策略（Runtime 性能优先）

- 蓝图（Gameplay 逻辑）应优先收敛到 **Native C++ 类**（更好的运行时性能、更可控的版本治理与审计）。
- UEIR JSON 的定位：用于“不可/不宜 C++ 化”的资产域，或用于离线审计与可视化差异。
- 因此后续投入的优先级应调整为：
  - 蓝图逻辑：Blueprint → C++（Gen/Degrade/审核）优先
  - 材质/特效/配置类资产：JSON 导出/还原优先（Material、Niagara、MIC、DataAsset 等）

2) UEIR 的“可逆”目标按资产域拆分

- 对蓝图：不追求 100% 图可逆；目标是“可生成可编译的 C++ 骨架 + 可审计的差异报告”
- 对非代码资产：逐步增强“JSON 可逆重建”与“空值/覆盖语义”

## 当前落地范围（v0.1.x）

代码入口与能力点（以实现为准）：

- UEIR Document v1（资产/蓝图/类）导出：仅定位信息 + 蓝图摘要（父类/变量类型摘要/函数图名）
- UEIR JSON → Markdown 报告（存在性校验、统计）
- 蓝图函数图子集导出/导入（K2 节点子集）+ 导入报告
- Blueprint Function Graph → C++（降级生成）+ 报告
- 专用 JSON roundtrip 示例：
  - MaterialInstanceConstant 参数导入/导出（MIC JSON）
  - 示例 DataAsset 导入/导出（ExampleDataAsset JSON）
  - Niagara User Params 导入/导出（Niagara User Params JSON）

## JSON 格式策略（建议调整）

目标是对外只暴露“源资产类型”，由插件自动选择解析与还原逻辑。

- 对外约定：只要输入一个 JSON 文件，插件即可根据外层的 `classPath`（或等价字段）自动分发到对应的 Importer
- 不同资产类型的 `payload` 结构不要求一致；导出与导入逻辑按资产类型特化，保证“写出的结构=读回的结构”
- 推荐形态：统一 Envelope + payload
  - `objectPath/classPath`：源资产定位与类型（用于路由与报告）
  - `payload`：资产域的具体内容（MIC 参数、Niagara User Params、蓝图函数图 IR、通用反射属性等）
  - `formatVersion`（可选）：同一资产类型内部格式演进时，用于兼容旧文件（不作为“类型区分”，只作为版本）

## 支持进度一览：Editor 下“资产 → JSON”

| 源资产类型 | 导出 JSON 支持进度 | 主要缺口 |
|---|---:|---|
| 任意 UObject 资产 | ✅（仅 objectPath/classPath/kind=Asset） | 缺少属性值/默认值/子对象树/依赖信息 |
| Material（UMaterial） | 🟡（已支持子集：常用表达式+连线+部分根输入） | 未覆盖复杂表达式类型、函数调用、材质属性全集与更完整的图语义 |
| 蓝图 UBlueprint（资产级） | 🟡（父类/变量类型摘要/函数图名） | 缺少组件树、默认值、事件图/宏图结构、函数图细节 |
| 蓝图函数图（指定函数） | 🟡（节点子集+pin 默认值+连线） | 节点支持面窄；复杂图大量缺失 |
| UClass（Native/Generated） | ✅（仅 classPath/kind=Class） | 缺少类成员/属性/函数签名 |
| MIC（MaterialInstanceConstant） | ✅（参数/贴图/Parent） | 仅覆盖少数参数类型；缺少静态开关等 |
| Niagara（User Params） | ✅（User Params） | 更复杂的数据接口/绑定/模块栈不覆盖 |
| DataAsset（示例） | ✅（ExampleDataAsset） | 仅示例类型；需要通用化框架 |

## 支持进度一览：JSON “还原/生成” 对应资产

| JSON 输入 | 还原目标 | 支持进度 | 备注/瓶颈 |
|---|---|---:|---|
| UEIR Document v1 | 任意 UObject 资产 | 🧪 | 当前 UEIR Document 不含属性值，无法还原 |
| UEIR Document v1 | 蓝图资产（按父类创建） | 🧪 | 需定义“蓝图生成规格”（组件/默认值/图）与可审计报告 |
| UEIR Blueprint Function Graph（结构体） | 导入到指定蓝图/创建新蓝图并导入 | 🟡 | API 已有，但缺少“从文件读 JSON→结构体→导入”的无人值守入口 |
| Material JSON（域专用 payload） | 覆盖/生成 Material | 🟡 | 目前仅支持表达式子集与部分根输入；缺失项会在报告中体现并影响可逆性 |
| MIC JSON（域专用 payload） | 覆盖/生成 MIC | ✅ | 可作为后续“资产域 JSON 可逆”的模板 |
| Niagara User Params JSON（域专用 payload） | 覆盖写回 NiagaraSystem | ✅ | 仍需更多类型与校验/报告 |
| ExampleDataAsset JSON | 覆盖写回 DataAsset | ✅（仅示例） | 需抽象成“通用 DataAsset 反射序列化” |

## 蓝图 ↔ C++ 互转进度（以“性能优先”为目标）

| 方向 | 目标产物 | 支持进度 | 备注/瓶颈 |
|---|---|---:|---|
| Blueprint Function Graph → C++ | `.generated.h/.generated.cpp` + 报告 | 🟡 | 仅子集节点与有限类型映射；需要更强降级策略与可编译保障 |
| C++ UFunction → Blueprint 包装函数图 | Blueprint 函数图（Entry/Call/Return） | 🟡 | 适合“把稳定 C++ API 暴露给蓝图”，不等同于逻辑还原 |
| Blueprint（资产级）→ C++ 类 | `UCLASS` + 组件/默认值/事件绑定骨架 | 🧪 | 当前未实现，需要定义导出粒度与“可编译”约束 |
| C++ 类 → Blueprint（可选） | 基于父类创建 BP，生成最小可用资产 | 🧪 | 建议作为工具链补齐而非核心路径 |

## 里程碑建议（按优先级排序）

### M1：建立“可推进的规格与验证”底座

- 明确 UEIR/域专用 JSON 的输入输出契约：版本、覆盖语义（字段缺失 vs null）、错误码与报告格式
- 建立“按资产类型分发”的域专用导出/导入框架
  - 外层 Envelope 统一（至少包含 objectPath/classPath/kind），插件根据 classPath 路由到对应 Exporter/Importer
  - 内层 payload 按资产类型特化；导出结构与导入读取结构必须严格一致
  - 先以 Material/Texture2D/StaticMesh 等典型资产定义字段白名单与最小可用导出粒度
  - Editor 工具：增加“导出域专用 JSON / 从域专用 JSON 导入（覆盖/创建）”入口，并与结果JSON落盘一致化
- 增加统一的执行结果 JSON（机器可读），用于 Skill 汇总与审计
  - Commandlet：支持 `OutResultJson=` 显式落盘
  - Editor：任务成功后弹窗询问，用户确认后选择保存路径落盘
- 为关键路径加入最小集自动化用例（命令行/Editor Utility），确保每次迭代可回归验证

#### M1 手工验证步骤

1) Commandlet（以 Export/Report 为例）

```powershell
& UnrealEditor-Cmd.exe YourProject.uproject -run=UEIRInterchange -unattended -UTF8Output `
  Mode=Export Assets="/Game/BP/BP_A.BP_A" OutDir="D:\ueir_out" OutFile="D:\ueir_out\export.json" OutResultJson="D:\ueir_out\export.result.json"

& UnrealEditor-Cmd.exe YourProject.uproject -run=UEIRInterchange -unattended -UTF8Output `
  Mode=Report Input="D:\ueir_out\export.json" OutDir="D:\ueir_out" OutFile="D:\ueir_out\report.md" OutResultJson="D:\ueir_out\report.result.json"
```

2) Editor

- Content Browser 选中资产 → Tools → UEIR → 导出所选资产为 UEIR JSON（或单文件导出）
- 成功通知后选择“保存结构化结果JSON”，确认保存路径，检查 result.json 是否生成
- Tools → UEIR → 从 UEIR JSON 生成报告，成功后同样保存 result.json

### M2：蓝图逻辑 → C++（性能优先）链路可用

- 扩展 GenCpp：保证“输出可编译”优先于“等价还原”，并把降级原因写入报告
- 扩展函数图导出覆盖面（先补高频控制流与数据构造节点），并对不支持节点提供可替代的降级模式
- 增加“从蓝图资产生成 C++ 类骨架”的模式（至少：UCLASS、组件声明、默认值初始化占位、关键事件绑定占位）

#### M2：Blueprint → C++ 可替代方案（目标与约束）

目标：对指定蓝图资产生成一个 Native C++ 类，使其在运行时可替代原蓝图类（不依赖蓝图 VM 执行逻辑），并输出可审计报告。

约束（满足才允许标记为“可替代”）：

- 生成的 C++ 必须可编译、可打包（不依赖 EditorOnly 模块）
- 蓝图事件/函数图必须达到 100% 可翻译覆盖（或处于白名单降级策略内），否则只生成“不可替代”的骨架与报告
- 不支持的节点/语义必须显式记录在报告中，且阻断“可替代”标记

输入/输出：

- 输入：Blueprint 对象路径（`/Game/.../BP_X.BP_X`）
- 输出：
  - `BP_X_Native.generated.h/.cpp`（或用户指定类名）
  - `BP_X_Native.codegen.md`（覆盖率、降级原因、对应图与节点定位）
  - 可选：`BP_X_Native.result.json`（结构化执行结果）

核心流程：

1) 资产级信息提取（用于类骨架）
   - ParentClass、Interfaces、Components（SCS）、变量列表与类型、默认值（CDO）
   - 构造：生成 `UCLASS` 继承自 ParentClass；在构造函数里创建默认子对象并应用默认值
2) 图级翻译（用于逻辑替代）
   - 对每个事件/函数图导出 IR（现有 Blueprint Function Graph IR 作为基础，逐步扩展节点支持面）
   - 将 IR 翻译为 C++ 语句块（控制流/条件/赋值/函数调用/返回）
   - 对 latent/宏/复杂容器/不支持节点：写入报告并阻断“可替代”
3) 绑定与生命周期对齐
   - 事件图入口映射：BeginPlay/Tick/Overlap/Input 等映射到对应 C++ override 或组件委托绑定
   - 变量访问与副作用：Blueprint 变量读写映射到 UPROPERTY 成员与必要的 OnRep/通知
4) 验证与交付
   - 编译级：保证生成文件可编译
   - 运行级：提供最小 smoke test（可选自动化测试/关卡回放对比）
   - 迁移：提供“将关卡中 Actor 重定向到新类”的操作建议与报告

建议的节点扩展优先级（以替代常见 Gameplay BP）：

- 控制流：Sequence、Branch、ForEachLoop、WhileLoop、DoOnce、Gate
- 数据：Make/Break Struct、Select、Clamp、Lerp、基本算术与比较
- 容器：Array Add/Remove/Find/Contains/Length（先覆盖 Array）
- 调用：CallFunction（静态/纯函数优先）、事件派发（delegate/broadcast）

### M3：非 C++ 资产域 JSON 可逆（Material/Niagara/DataAsset）

- MIC：补齐常用参数类型与静态开关；明确“清空/覆盖”的语义并输出差异报告
- Niagara：扩展到更通用的 User Params 类型/数据接口；提供校验与失败定位
- DataAsset：抽象成“通用反射序列化框架”（白名单类型、软引用、数组/Map/Set、嵌套结构体），并支持空值/覆盖语义

### M4：无人值守导入（批处理）能力

- 在现有 Commandlet 增加 `Mode=ImportBlueprintGraph`：读 JSON 文件 → 解析 → 导入/创建 → 写报告 → 以退出码返回
- 增加“导入计划 JSON”（一次输入可包含多个资产操作），在 UE 内部循环执行，减少外部编排成本

## 主要瓶颈与对策

| 瓶颈 | 影响 | 对策 |
|---|---|---|
| K2 节点支持面窄 | 导出不完整、导入/GenCpp 失败率高 | 优先补控制流与常用数据节点；建立降级策略与报告 |
| 依赖硬加载（StaticLoadObject） | 批量处理慢、易受工程状态影响 | 引入 AssetRegistry/SoftObjectPath 优先、延迟加载与缓存 |
| “空字段/覆盖语义”不明确 | 无法可靠重建与做差异 | schema 里明确：缺失=不改，null=清空，显式值=覆盖 |
| UEIR Document 过于摘要 | “资产还原”无法落地 | 按资产域拆分：通用反射 JSON vs 域专用 JSON（MIC/Niagara/…） |

## Skill 优化方向（与插件配套）

- 让 LLM 产出“导入计划 JSON/UEIR JSON/C++ 源码”作为唯一输出，避免多轮工具调用
- 插件/命令行负责：校验 → 执行 → 报告 → 汇总结果（结构化 JSON + Markdown）
- 蓝图策略：优先生成 C++（性能）；仅在编辑体验或域资产需要时输出 JSON 并还原
