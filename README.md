# Synera: Synergy Auto-Arena

本仓库当前实现 PA 第一阶段逻辑核心、阶段二 PvE 战斗核心、阶段三经营成长系统与 Qt Widgets GUI。代码采用 C++ OOP 风格，把棋盘、备战区、单位、玩家、敌方轮次生成、战斗状态机、商店、装备、羁绊和存档拆成独立模型，GUI 只通过 `GameState` 公开 API 触发状态变化。

## 阶段一完成度

- [x] M x N 棋盘、玩家半场/敌方半场与地块占用规则
- [x] Bench 与棋盘之间的数据同步
- [x] `Unit` 基类，包含 HP/ATK/Range/Max Mana/Mana 等字段
- [x] 我方/敌方统一 `Unit` 类型，仅用 `owner` 区分控制归属
- [x] `traits` 仅作为羁绊标签，不参与敌我判断
- [x] 玩家实体与敌方轮次生成 `Er`
- [x] Qt GUI 展示棋盘/备战区/单位信息，并支持玩家单位拖拽摆放

## 阶段二完成度

- [x] Core 层严格阶段机：`Prep -> Combat -> Resolve -> Prep/GameOver`
- [x] `startCombat()` / `tickCombat()` / `resolveRound()` 返回 `ActionResult`
- [x] Combat / Resolve / GameOver 阶段 core 层拒绝部署、移动与回收上场单位
- [x] 开战时原子生成本轮敌人 Er，并保存玩家上场快照
- [x] 帧驱动自动索敌、攻击、回蓝、技能、死亡清格与胜负检测
- [x] BFS 四方向寻路到可攻击空格，并使用两阶段移动提案避免重叠
- [x] `Vanguard` / `SparkMage` / `IrisGuard` 三种多态英雄技能
- [x] 结算时清理敌人、恢复玩家上场单位、更新金币/血量/回合/胜负

## 阶段三完成度

- [x] 初始免费 5-slot 商店，支持购买、手动刷新与金币扣除
- [x] 人口上限、人口升级费用、Bench 不受人口限制
- [x] 5 种羁绊：`Warrior` / `Guardian` / `Mystic` / `Ranger` / `Healer`
- [x] 1 星三合一到 2 星，保留最新获得单位并返还被合并单位装备
- [x] 4 种基础装备：铁剑、锁子甲、急速手套、蓝水晶
- [x] `Prep` / `Resolve` / `GameOver` 存档，`Combat` 中拒绝保存
- [x] GUI 展示商店、装备栏、羁绊、星级、人口、金币、轮次与阶段

## 文件结构

```text
src/
  main.cpp                  命令行 smoke demo
  core/
    Types.*                 公共枚举、UnitId、坐标与位置类型
    Unit.*                  Unit 基类与 BasicUnit
    Player.*                玩家经营实体
    Board.*                 M x N 棋盘与半场规则
    Bench.*                 一维备战区
    Catalog.*               英雄与装备定义、Unit factory
    UnitManager.*           UnitId 到 Unit 对象的集中管理
    GameState.*             统一执行 Bench/Board/Unit 三方同步
    EncounterGenerator.*    按轮次生成敌方集合 Er
    SkillContext.*          技能受控访问战场的上下文
  gui/
    MainWindow.*            Qt 主窗口、状态栏与刷新协调
    PlacementController.*   将 drop 请求转换为 GameState API 调用
    BoardWidget.*           只读绘制棋盘与处理棋盘 drop
    BenchWidget.*           只读绘制备战区与处理 Bench drop
    ShopPanel.*             商店 offer、购买与刷新
    EquipmentPanel.*        装备栏与装备选择
    SynergyPanel.*          羁绊计数、阈值与效果展示
    InspectorPanel.*        只读显示选中单位属性
    AssetManager.*          加载并缓存 GUI 图片资源
tests/
  test_stage1.cpp           阶段一断言测试
  test_stage2.cpp           阶段二断言测试
  test_stage3.cpp           阶段三断言测试
```

## 核心设计

- `UnitManager` 独占所有 `Unit` 对象，`Board` 与 `Bench` 只保存 `UnitId`，避免单位被复制后状态不同步。
- `Board` 与 `Bench` 的写入函数是私有的，所有移动通过 `GameState` 完成，保证容器占用和 `Unit::placement` 原子更新。
- 默认棋盘为 8 x 8，Bench 为 8 格，也支持构造自定义尺寸。
- 坐标使用 0-based `Position{row, col}`。敌方半场为 `row < rows / 2`，玩家半场为 `row >= rows / 2`。
- `Owner::PlayerCtrl` 与 `Owner::EnemyCtrl` 是唯一敌我来源，`traits` 只保留给后续羁绊系统。
- `UnitStats` 固定包含 `maxHp` / `atk` / `range` / `maxMana` / `attackInterval` / `moveInterval`。单位保存 `baseStats` 与 `effectiveStats`，所有装备、星级、羁绊效果都通过 `computeEffectiveStats` 从基础值重算，不在旧有效属性上叠加。
- `PlacementPolicy::Reject` 用于非法放置回弹，`PlacementPolicy::Swap` 用于后续 GUI 拖拽交换。
- GUI 展示 widget 只持有 `const GameState*`，不缓存 `Unit` 副本，也不直接修改 `Board` / `Bench` / `UnitManager`。
- `PlacementController` 是拖拽落点到核心 API 的唯一转换层，失败时不做任何状态补偿，只返回 message。

## 阶段二战斗规则

- `GamePhase::Prep` 允许排兵布阵；`Combat` 由 AI 接管；`Resolve` 只允许结算；`GameOver` 后部署、战斗、结算 API 均拒绝并返回失败。
- `startCombat()` 只能在 Prep 调用；`tickCombat()` 只能在 Combat 调用；`resolveRound()` 只能在 Resolve 调用。非法阶段调用返回 `{false, message}` 且不修改状态。
- `Ar` 是当前棋盘上 `owner == PlayerCtrl && alive` 的单位；`Er` 是本轮 `startCombat()` 生成过的 `EnemyCtrl` 单位。Bench 中单位不属于 Ar，不参与战斗、不被索敌、不被攻击。
- `currentEnemyUnits()` 返回本轮生成过的全部 Er，包括 Dead；`activeEnemyUnits()` 只返回当前棋盘上 alive 的 EnemyCtrl。`resolveRound()` 后 `currentEnemyUnits()` 清空。
- 若开战时 Ar 为空，本轮直接判负进入 Resolve；失败但玩家未死亡时不推进 round，回到 Prep 重打当前关。只有胜利才推进 round；最终关胜利进入 GameOver。
- 距离统一为 `distSq = (row1-row2)^2 + (col1-col2)^2`。索敌和攻击范围都使用 `distSq <= range * range`；`range = 1` 只覆盖上下左右相邻格，不覆盖斜对角。
- 索敌 tie-break：`distSq` 小优先；距离相同 HP 高优先；再 `col` 小优先；再 `row` 大优先；最后 `UnitId` 小优先。
- BFS 只按四方向移动。目标不在攻击范围时，寻路目标集合是所有可达、空、且进入攻击范围的格子；目标单位所在格不可进入。Pathfinder 只返回下一步。
- 每帧先复制 `activePlayerUnits() + activeEnemyUnits()` 并按 UnitId 排序，再更新独立 `CombatRuntime` 冷却。攻击/技能事件先收集，先结算 damage，再结算 heal，最后统一死亡清格；随后应用移动提案。
- 普攻命中后才回蓝。若单位行动时 `mana >= maxMana`，本次行动释放技能并清零，不普攻；普攻把 mana 打满时不会同帧立刻放技能。
- 每个单位每帧最多提出一个 next cell；目标格必须当前为空。多个单位申请同一格时全部不移动，应用前再次检查目标格仍为空。
- 死亡单位设置 `hp = 0`、`state = Dead`，并从 Board 占用表清除，但 tick 中不从 `UnitManager` 删除。EnemyCtrl 统一在 `resolveRound()` 删除。

## 阶段三经营规则

- `refreshShop` / `purchaseShopSlot` / `upgradePopulation` / `equipItem` 只能在 `Prep` 阶段调用；其它阶段返回失败且不修改状态。
- `GameState` 初始化时自动生成一次免费 5-slot 商店。之后 `refreshShop()` 花费 2 金币，金币不足时失败且商店不变。购买成功后扣金币、创建 `PlayerCtrl` 单位放入 Bench、触发升星检查，并将被购买 slot 置空。
- 初始等级为 1，人口上限为 3。人口只限制 Board 上 `owner == PlayerCtrl` 的单位数量，Bench 不受限制。升级费用为 `4 + 2 * (level - 1)`，每级人口 +1，最高等级 6，最高人口 8。
- 羁绊只统计 Board 上的玩家单位，Bench 与敌方单位不计入。同一个 `definitionId` 对每个 trait 只贡献 1 次计数。开战时冻结本轮羁绊，战斗中单位死亡不会导致羁绊跳变。
- 羁绊效果：`Warrior` 2/4 给 Warrior ATK +10/+25；`Guardian` 2/4 给 Guardian MaxHP +100/+250；`Mystic` 2/3 给 Mystic MaxMana -10/-20，最低 10；`Ranger` 2 给 Ranger attackInterval -10%；`Healer` 2 让治疗效果 +20%。
- 升星只实现 1 星三合一到 2 星。合并时保留 `acquireSeq` 最大的单位及其位置，删除另外两个；被删除单位身上的装备返回装备栏。2 星倍率固定 1.7，只影响 HP 与 ATK，不影响 range、maxMana、attackInterval。
- 装备包含铁剑 ATK +15、锁子甲 MaxHP +150、急速手套 attackInterval -20%、蓝水晶 MaxMana -30。每个单位最多 1 件装备，已有装备时穿戴失败。蓝水晶不会把 maxMana 降到 10 以下，急速手套只影响穿戴者。
- 胜利结算时按 `CombatConfig::itemDropPercent` 掉落基础装备；测试中可设置为 0 或 100，避免随机性。

## 存档规则

- `saveToFile` / `loadFromFile` 返回 `ActionResult`。`saveToFile` 允许 `Prep` / `Resolve` / `GameOver`，`Combat` 中返回失败。
- 存档第一行为 `SYNERA_SAVE 1`。文件保存玩家状态、阶段、轮次、`nextUnitId`、`nextItemId`、商店、装备、单位 `definitionId/unitId/owner/star/acquireSeq/hp/mana/placement/equippedItemId`。
- `loadFromFile` 先读入临时 `GameState`，校验版本、definitionId、itemDefId、UnitId、ItemId、placement 与装备归属，成功后再整体替换当前状态。
- 读档后会重建 Board/Bench 占用、装备归属、羁绊与所有 effectiveStats，并清空 combat runtime。`effectiveStats` 不作为权威存档状态。

## GUI 拖拽规则

- Qt Drag & Drop payload 使用 `QByteArray + QDataStream`，包含 `UnitId`、来源类型、来源 Bench slot 或 Board position。
- 只有 `owner == PlayerCtrl` 且 GUI 阶段为 `Prep` 的单位可以拖动；敌方单位只能选中查看。
- Bench 到空 Board：调用 `deployFromBench(..., Reject)`。
- Bench 到占用 Board：阶段一直接 Reject，避免目标单位无处可去。
- Board 到空己方 Board：调用 `moveBoardUnit(..., Reject)`。
- Board 到己方占用 Board：调用 `moveBoardUnit(..., Swap)`，交换两个单位位置。
- Board 到空 Bench：调用 `returnToBench`；目标 Bench 已占用则 Reject。
- 非法放置保证 `Board` 占用、`Bench` 占用和 `Unit::placement` 不变，状态栏显示 `Invalid placement` 或更具体提示。
- 装备交互为先在装备栏选中装备，再点击棋盘或 Bench 上的玩家单位进行穿戴。Combat 阶段禁用商店、刷新、升级、装备和保存按钮。

## 资源目录

GUI 会从可执行文件旁的 `assets/` 和项目根目录的 `assets/` 查找 PNG/JPG/JPEG 图片。`Unit::visualKey` 只用于 GUI 查找素材，不参与 owner、traits、战斗或部署规则。

`AssetManager` 会缓存 `QPixmap`，不会在 `paintEvent()` 中反复读文件。没有 `assets/` 目录或找不到素材时，GUI 自动回退到纯色方块、文本、HP 条和 Mana 条。当前阶段不要求 GIF 动画，后续可用 `QMovie` 扩展。

## 编译与运行

当前环境可用 MSYS2 g++ 编译命令行版本：

```powershell
& 'D:\Apps\MSYS2\ucrt64\bin\g++.exe' -std=c++20 -Wall -Wextra -pedantic -Isrc src\main.cpp src\core\*.cpp -o synera_demo.exe
.\synera_demo.exe
```

运行测试：

```powershell
& 'D:\Apps\MSYS2\ucrt64\bin\g++.exe' -std=c++20 -Wall -Wextra -pedantic -Isrc tests\test_stage1.cpp src\core\*.cpp -o stage1_tests.exe
.\stage1_tests.exe
```

测试程序无输出即表示所有断言通过。

CMake 构建后也可以运行三阶段测试：

```powershell
.\build\stage1_tests.exe
.\build\stage2_tests.exe
.\build\stage3_tests.exe
```

Qt GUI 推荐使用 Qt 自带 CMake 和 MinGW：

```powershell
& 'D:\Apps\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH='D:\Apps\Qt\6.11.1\mingw_64' `
  -DCMAKE_CXX_COMPILER='D:\Apps\Qt\Tools\mingw1310_64\bin\g++.exe' `
  -DCMAKE_MAKE_PROGRAM='D:\Apps\Qt\Tools\mingw1310_64\bin\mingw32-make.exe'
& 'D:\Apps\Qt\Tools\CMake_64\bin\cmake.exe' --build build
$env:PATH='D:\Apps\Qt\6.11.1\mingw_64\bin;D:\Apps\Qt\Tools\mingw1310_64\bin;' + $env:PATH
.\build\synera_gui.exe
```

## AI 使用说明

本阶段由 AI 辅助完成工程规划与代码初稿。核心模块包括：

- `GameState`：集中处理 Bench、Board、Unit、Shop、Equipment、Synergy 和 Save/Load 的状态同步，避免 GUI 或其它系统直接修改占用表。
- `EncounterGenerator`：根据当前轮次生成 `EnemyCtrl` 单位并部署到敌方半场，形成文档中的 `Er`。
- `Catalog`：保存英雄与装备定义，商店和存档只引用稳定 definition id，实际单位由 factory 创建。

后续继续开发时，需要确保自己能解释每个核心类的职责、阶段限制、属性重算流程和存档校验流程，再在其上添加扩展经济、装备合成、音效或其它阶段四内容。
