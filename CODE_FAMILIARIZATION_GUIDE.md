# PvZ Auto Arena 代码熟悉指南

这份指南用于快速理解当前代码。项目仍名为 Synera，但内容主题是 PvZ 自走棋。

## 阅读顺序

1. `src/core/Types.h`：公共类型、`UnitDefinition`、`BoardHalf`。
2. `src/core/Unit.h/.cpp`：`Unit` 基类和多态技能类。
3. `src/core/Catalog.cpp`：基础植物目录、二星视觉映射、装备、羁绊和显示 helper。
4. `src/core/GameState.h/.cpp`：唯一的游戏状态修改入口。
5. `src/core/EncounterGenerator.cpp`：僵尸模板和波次生成。
6. `src/gui/`：Qt 展示与交互层。
7. `tests/`：行为边界与验收点。

## 核心规则

- 玩家单位和僵尸单位都是 `Unit`，用 `Owner::PlayerCtrl` / `Owner::EnemyCtrl` 区分。
- `Board` 和 `Bench` 只保存 `UnitId`，真正对象由 `UnitManager` 拥有。
- GUI 不直接修改 Board、Bench 或 UnitManager，只调用 `GameState` API。
- 商店只出售基础植物：`peashooter`、`sunflower`、`wallnut`、`puffshroom`、`fumeshroom`、`spikeweed`。
- `repeater`、`twin_sunflower`、`tallnut`、`scaredyshroom`、`gloomshroom`、`spikerock` 只作为二星视觉资源。
- 升星不改变 `definitionId`、`traits`、`factoryKey` 或技能行为，只改变属性倍率和 `displayVisualKey()` 返回值。

## GUI 资源约定

- 单位实例显示使用 `displayVisualKey(const Unit&)`。
- 商店 offer 使用 `UnitDefinition.star1VisualKey` 和 `assets/shop_cards/`，不构造临时单位。
- 棋盘同屏分半场绘制：敌方半场 `backgrounds/night_board`，玩家半场 `backgrounds/day_board`。
- `EquipmentPanel` 使用 `ui/zombieline` 作为横向装备托盘，缺图时回退普通按钮列表。
- `assets/units/` 是单位视觉资源池，不是商店单位池。
- `assets/shop_cards/` 是商店种子包资源池，只服务基础可购买植物。

## 测试重点

- 进化体 ID 不进入 `unitCatalog()` 或 `shopOffers()`。
- 三个同基础单位合成后仍保留基础 `definitionId`，`star == 2`。
- 二星显示贴图来自 `star2VisualKey`，敌人显示自身 `visualKey`。
- 二星只改变属性和贴图，不改变羁绊贡献身份或技能类型。
- 资源校验覆盖基础贴图、二星贴图、商店卡牌、`ui/zombieline` 和日/夜背景。
