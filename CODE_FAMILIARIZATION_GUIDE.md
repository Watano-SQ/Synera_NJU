# PvZ Auto Arena 代码熟悉指南

这份指南帮助你快速理解当前代码。项目仍叫 Synera，但内容主题已经换成 PvZ。

## 阅读顺序

1. `src/core/Types.h`：公共类型、`UnitDefinition`、`ItemDefinition`、`TraitDefinition`。
2. `src/core/Unit.h/.cpp`：`Unit` 基类、`BasicUnit`、`PeaBurst`、`FumeLineCaster`、`SunHealer`。
3. `src/core/Catalog.cpp`：植物单位、装备、羁绊定义。
4. `src/core/GameState.h/.cpp`：唯一的游戏状态修改入口。
5. `src/core/EncounterGenerator.cpp`：僵尸模板和波次生成。
6. `tests/`：行为边界和验收点。
7. `src/gui/`：Qt 展示和交互层。

## 核心不变量

- 玩家单位和僵尸单位都是 `Unit`，只通过 `Owner::PlayerCtrl` / `Owner::EnemyCtrl` 区分。
- `traits` 只保存 ASCII traitId，例如 `shooter`、`sun`、`fungus`，不参与敌我判断。
- `Board` 和 `Bench` 只保存 `UnitId`，真正的对象由 `UnitManager` 拥有。
- 所有位置移动都必须同时更新 Board/Bench 占用表和 `Unit::placement()`。
- GUI 不直接修改 Board、Bench 或 UnitManager，只调用 `GameState` 的公开 API。

## 当前内容表

植物单位：

- `peashooter` 豌豆射手
- `repeater` 双重射手
- `sunflower` 向日葵
- `twin_sunflower` 双胞向日葵
- `wallnut` 坚果墙
- `tallnut` 高坚果
- `puffshroom` 小喷菇
- `fumeshroom` 大喷菇
- `gloomshroom` 忧郁菇
- `spikeweed` 地刺
- `spikerock` 地刺王

羁绊：

- `shooter` 射手：攻击间隔降低。
- `nut` 坚果：最大生命提高。
- `sun` 阳光：胜利结算额外获得阳光。
- `healer` 治愈：治疗效果提高。
- `fungus` 真菌：最大法力降低。
- `spike` 地刺：攻击力提高。

装备：

- `plant_food` 植物营养剂：攻击力 +15。
- `pumpkin_shell` 南瓜护壳：最大生命 +150。
- `garden_glove` 园艺手套：攻击间隔 -20%。
- `chlorophyll` 叶绿素：最大法力 -30。

## 战斗流程

`Prep -> Combat -> Resolve -> Prep/GameOver`

- `startCombat()` 冻结羁绊、重算属性、生成僵尸并记录玩家站位快照。
- `tickCombat()` 逐帧推进索敌、移动、攻击、回蓝、施法、死亡清格。
- `resolveRound()` 清理僵尸、恢复玩家单位、结算阳光、推进波次或结束游戏。

注意：`sun` 羁绊是经济机制，使用 `settlementGoldBonusFromSynergies()` 在胜利结算中处理；失败奖励不吃这个 bonus。

## GUI 边界

GUI 使用 `AssetManager` 从 `assets/` 读取 PNG/JPG 资源，visualKey 不带扩展名，例如 `units/peashooter`。

- `BoardWidget` 绘制草坪背景、半透明半场、格线、单位、血条、蓝条和交互 overlay。
- `ShopPanel` 显示植物头像、中文名、阳光费用和羁绊。
- `EquipmentPanel` 显示装备图标。
- `SynergyPanel` 通过 `traitCatalog()` 显示羁绊中文名和图标。
- `InspectorPanel` 只读展示单位详情。

若背景或贴图缺失，GUI 应回退到纯色绘制和文字占位。

## 资源

`pictures/` 是临时素材池，正式资源由脚本生成：

```powershell
python .\tools\build_assets.py
```

运行时不依赖 `pictures/`。

## 测试重点

- 单位目录和资源 key 可用。
- `PeaBurst`、`FumeLineCaster`、`SunHealer` 的技能行为仍通过。
- 6 个 PvZ 羁绊计数、去重、战斗冻结正确。
- `sun` 羁绊只增加胜利奖励。
- 4 件装备效果正确。
- 新 ID 可以正确存档、读档、恢复装备、星级和站位。
- 资源校验使用标准库文件系统，不让 core 依赖 Qt。
