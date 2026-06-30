# UEIR v1 IR 数据结构（草案）

UEIR v1 的设计目标是先把“可定位 + 可分类 + 可审阅”的信息稳定下来，后续再逐步补充“可逆/可重建”的细节（如蓝图图节点、资产依赖、默认值与子对象树）。

## 顶层：FUEIRDocument

- schemaId：Schema 标识，固定为 `ueir://schemas/v1/document.json`
- version：IR 版本（插件当前输出为 `1.0.0-draft`）
- meta：导出元信息
- items：条目列表（资产/蓝图/类）

## 元信息：FUEIRMeta

- exportedAtUtcIso8601：UTC 时间（ISO 8601）
- engineVersion：引擎版本字符串（`FEngineVersion::Current().ToString()`）
- projectName：工程名（`FApp::GetProjectName()`）
- pluginName / pluginVersion：插件标识

## 条目：FUEIRItem

- kind：`Asset | Blueprint | Class`
- objectPath：对象路径（`UObject::GetPathName()`）
- classPath：类路径（`Object->GetClass()->GetPathName()`；若 kind=Class 则为该类自身路径）
- blueprint：仅当 kind=Blueprint 时有意义（仍会以空对象序列化输出）

## 蓝图摘要：FUEIRBlueprintSummary

- parentClassPath：父类路径（`UBlueprint::ParentClass->GetPathName()`）
- variables：蓝图变量摘要数组
- functionGraphNames：函数图名称数组（`UBlueprint::FunctionGraphs`）

## 变量摘要：FUEIRVariable

- name：变量名
- type：PinCategory（如 `bool`/`int`/`float`/`object`/`struct` 等）
- container：`None | Array | Set | Map`
- subTypeObjectPath：当类型为 Object/Class/Struct/Enum 等时记录子类型对象路径（若可用）
- bEditable / bExposeOnSpawn：预留字段（v1 目前固定输出默认值）

## 示例（导出结果片段）

```json
{
  "schemaId": "ueir://schemas/v1/document.json",
  "version": "1.0.0-draft",
  "meta": {
    "exportedAtUtcIso8601": "2026-05-26T00:00:00Z",
    "engineVersion": "5.7.0-0+++UE5+Release-5.7",
    "projectName": "VRExpPluginExample",
    "pluginName": "UEIRInterchange",
    "pluginVersion": "0.1.0"
  },
  "items": [
    {
      "kind": "Blueprint",
      "objectPath": "/Game/BP/MyActor.MyActor",
      "classPath": "/Script/Engine.Blueprint",
      "blueprint": {
        "parentClassPath": "/Script/Engine.Actor",
        "variables": [
          { "name": "Speed", "type": "float", "container": "None", "subTypeObjectPath": "", "bEditable": false, "bExposeOnSpawn": false }
        ],
        "functionGraphNames": [ "UserConstructionScript" ]
      }
    }
  ]
}
```

