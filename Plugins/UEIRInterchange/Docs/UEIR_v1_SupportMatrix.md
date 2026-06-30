# UEIR Interchange v1 支持矩阵（草案）

本插件以 UEIR（Unreal Engine Interchange Representation）作为中间表示，目标是支撑 UE5 中 C++ 类型、蓝图与资产的可交换、可审阅、可版本化（JSON）描述。

支持级别定义：

- ✅ 支持：可在 v1 里端到端使用
- 🟡 部分：仅导出/仅子集/不保证可逆
- 🧪 草案：接口/数据结构已预留但未实现
- ❌ 不在 v1：明确不做

| 源 → 目标 | UEIR JSON（文件） | UE 资产（UObject） | UE 蓝图（UBlueprint） | UE C++（源码） |
|---|---:|---:|---:|---:|
| UE 资产（任意 UObject 资产） | ✅ 导出（路径+类） | 🧪 导入 | 🧪 导入 | ❌ |
| UE 蓝图（UBlueprint 资产） | 🟡 导出（父类+变量摘要+函数图名） | 🧪 导入 | 🧪 导入 | ❌ |
| UE 类（UClass，含 Native/Generated） | ✅ 导出（类路径） | 🧪 导入 | 🧪 导入（基于父类创建 BP） | ❌ |
| UEIR JSON（文件） | N/A | 🧪 | 🧪 | ❌ |

v1 最小可用范围（本仓库已落地）：

- Content Browser 选中资产 → UEIR JSON 文件导出
- UEIR 文档头（版本/元信息）+ Item 列表
- 蓝图资产额外导出：父类路径、变量类型摘要、函数图名称列表

