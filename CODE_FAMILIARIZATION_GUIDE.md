# Synera 代码熟悉指南

这份指南的目标不是再写一遍功能说明，而是帮你从零开始掌控这份代码：知道先读哪里、每个模块管什么、哪些不变量不能破坏、调试时从哪个入口切进去。GUI 相关代码会在最后单独说明，因为后续要替换 GUI，核心逻辑不应和现有 Qt 实现绑死。

## 0. 先建立项目地图

项目由三块组成：

- `src/core/`：真正的游戏逻辑。棋盘、备战区、单位、玩家、战斗、商店、装备、羁绊、存档都在这里。
- `src/gui/`：Qt Widgets 界面。它展示 core 状态，并把点击、拖拽、按钮操作翻译成 `GameState` API 调用。
- `tests/`：三阶段断言测试。它们是理解行为边界的最好入口。

`PA说明文档.pdf` 给出的核心要求可以概括为：做一个 PvE auto-battler，必须有棋盘、备战区、单位属性、敌我归属、羁绊、经营成长、自动战斗状态机，以及 GUI 展示。当前源码已经把这些要求拆到了 core 和 gui 两层，后续替换 GUI 时不要动 core 的职责边界。

建议阅读顺序：

1. `src/core/Types.h`
2. `src/core/Unit.h` / `src/core/Unit.cpp`
3. `src/core/Board.h` / `Bench.h`
4. `src/core/GameState.h`
5. `src/core/GameState.cpp`
6. `tests/test_stage1.cpp`
7. `tests/test_stage2.cpp`
8. `tests/test_stage3.cpp`
9. 最后再读 `src/gui/`

## 1. 第一层：公共类型

先读 `src/core/Types.h`。这里定义了几乎所有模块共享的词汇。

必须记住的类型：

- `UnitId` / `ItemId`：所有单位和装备实例都用整数 ID 互相引用。
- `Owner`：`PlayerCtrl` 和 `EnemyCtrl`，这是唯一敌我判断来源。
- `UnitState`：`Idle`、`Moving`、`Attacking`、`Casting`、`Dead`。
- `GamePhase`：`Prep`、`Combat`、`Resolve`、`GameOver`。
- `Position`：棋盘坐标，使用 0-based 的 `{row, col}`。
- `Placement`：单位当前位置，可以是无位置、Bench 格子、Board 格子。
- `UnitStats`：基础战斗属性，包含 HP、ATK、Range、Mana、攻击间隔、移动间隔。
- `ActionResult`：带 `success` 和 `message` 的操作结果，主要用于战斗、经营、存档等可能失败的 API。

一个重要习惯：不要把 trait 当敌我阵营。trait 只服务羁绊，敌我只能看 `Owner`。

## 2. 第二层：数据所有权

这份代码的核心设计是：`UnitManager` 拥有单位对象，`Board` 和 `Bench` 只保存 `UnitId`。

### Unit

`src/core/Unit.h` 和 `src/core/Unit.cpp` 定义单位。

`Unit` 保存：

- `definitionId`：来自图鉴/商店/存档的稳定定义 ID。
- `owner`：敌我控制权。
- `traits`：羁绊标签。
- `baseStats`：基础属性。
- `effectiveStats`：星级、装备、羁绊计算后的最终属性。
- `hp` / `mana` / `state`。
- `placement`：当前在 Bench、Board，还是无位置。
- `equippedItemId`：身上装备实例 ID。

关键点：

- `Unit` 禁止拷贝，避免复制后状态不同步。
- `setHp(0)` 会把状态置为 `Dead`。
- `setEffectiveStats()` 会修正非法属性下界，并根据 max HP / max Mana 变化调整当前 HP / Mana。
- 技能通过虚函数 `castSkill(SkillContext&)` 实现，目前有 `BasicUnit`、`Vanguard`、`SparkMage`、`IrisGuard`。

读代码时要能回答：

- 为什么 `Unit` 同时有 `baseStats` 和 `effectiveStats`？
- 为什么装备、星级、羁绊不能直接永久叠加到 `effectiveStats` 上？
- `Dead` 和 `hp == 0` 是如何互相影响的？

### UnitManager

`src/core/UnitManager.*` 是单位对象池。

它负责：

- 分配递增 `UnitId`。
- 根据 ID 查找单位。
- 删除单位。
- 存档读档时用指定 ID 插入单位。

它不负责：

- 判断单位能不能上场。
- 判断单位在哪个格子。
- 更新 Bench / Board 占用。

这些都归 `GameState`。

### Board 和 Bench

`Board` 是二维棋盘，`Bench` 是一维备战区。

它们共同特点：

- 公开接口基本只读。
- 真正写入函数是 private。
- `GameState` 是 friend，因此只有 `GameState` 能改占用表。

这是本项目最重要的不变量之一：单位移动时必须同时更新三处状态。

- `Board` 或 `Bench` 的占用表。
- `Unit::placement()`。
- 必要时还要更新羁绊和有效属性。

所以不要在外部直接改 `Board` / `Bench`，也不要只改 `Unit::placement()`。

## 3. 第三层：GameState 是唯一核心门面

`src/core/GameState.h` 是你最应该背熟的文件。它把玩家、棋盘、备战区、单位池、阶段、商店、装备、羁绊、敌人、战斗运行时都收在一起。

把 `GameState` 分成四类 API 来读：

### 3.1 查询类

典型函数：

- `player()` / `board()` / `bench()` / `units()`
- `phase()` / `lastRoundResult()` / `matchResult()`
- `activePlayerUnits()` / `activeEnemyUnits()` / `activeCombatUnits()`
- `shopOffers()` / `equipmentInventory()` / `activeSynergies()`

这些函数帮助 GUI、测试、命令行 demo 观察状态。

### 3.2 布阵类

典型函数在 `GameState.cpp` 中：

- `addUnitToBench`
- `deployFromBench`
- `moveBoardUnit`
- `returnToBench`

核心规则：

- 只有 `Prep` 阶段能部署、移动、回收上场单位。
- 玩家单位只能放在玩家半场。
- 敌方单位只能生成在敌方半场。
- 人口只限制玩家棋盘上单位数，不限制 Bench。
- `PlacementPolicy::Reject` 遇到占用格直接失败。
- `PlacementPolicy::Swap` 用于交换两个玩家单位的位置。

你读这几个函数时，重点看它们如何保证原子性：失败时不能留下半更新状态。

### 3.3 战斗类

典型函数：

- `startCombat`
- `tickCombat`
- `resolveRound`

阶段流是：

```text
Prep -> Combat -> Resolve -> Prep
                   Resolve -> GameOver
```

`startCombat()` 做这些事：

- 只能在 `Prep` 调用。
- 根据当前回合生成敌人。
- 记录玩家上场单位快照。
- 冻结当前羁绊和属性。
- 初始化每个战斗单位的运行时冷却。
- 如果没有玩家上场单位，直接判负进入 `Resolve`。

`tickCombat()` 是帧驱动战斗：

- 只能在 `Combat` 调用。
- 收集当前活着的玩家和敌方单位。
- 更新攻击/移动冷却。
- 为每个单位选择目标。
- 如果目标在攻击范围内，普攻或施法。
- 如果目标不在范围内，用 BFS 找下一步。
- 先结算伤害，再结算治疗，再清死者，再应用移动提案。
- 如果一方没有活跃单位，进入 `Resolve`。

`resolveRound()` 做结算：

- 胜利给金币、可能掉装备、推进回合。
- 失败扣玩家 HP、给少量金币、当前回合不推进。
- 清理敌人。
- 恢复玩家开战前站位，HP 回满，Mana 清零，状态回 `Idle`。
- 最终胜利或玩家 HP 为 0 时进入 `GameOver`。

读战斗代码时要特别注意：

- Bench 中单位不参战。
- `currentEnemyUnits()` 包含本轮生成过的敌人，死了也保留到结算。
- `activeEnemyUnits()` 只返回棋盘上活着的敌人。
- 攻击范围用平方距离：`distSq <= range * range`。
- 移动只允许四方向 BFS。
- 多个单位申请同一目标格时全部不移动。

### 3.4 经营、成长、存档类

经营相关函数：

- `refreshShop`
- `purchaseShopSlot`
- `upgradePopulation`
- `equipItem`
- `computeEffectiveStats`
- `maybeMergeUnit`
- `computeSynergiesFromBoard`

经营规则：

- 商店固定 5 个 slot。
- 初始化时免费刷一次商店。
- 后续刷新花 2 金币。
- 购买成功后单位进入 Bench，并触发升星检查。
- 人口升级费用为 `4 + 2 * (level - 1)`。
- 最高等级 6，最高人口 8。
- 装备只能在 `Prep` 阶段穿，每个单位最多 1 件。

升星规则：

- 目前只实现 1 星三合一到 2 星。
- 保留 `acquireSeq` 最大的最新单位。
- 删除另外两个单位。
- 被删除单位的装备回到装备栏。
- 2 星只影响 HP 和 ATK，倍率 1.7。

羁绊规则：

- 只统计 Board 上的玩家单位。
- Bench 不计入。
- 敌方单位不计入。
- 同一个 `definitionId` 对同一个 trait 只贡献一次计数。
- 开战时羁绊冻结，战斗中单位死亡不会让羁绊跳变。

存档相关函数：

- `saveToFile`
- `loadFromFile`

存档规则：

- `Combat` 中不允许保存。
- 读档不允许读入 `Combat` 阶段存档。
- 读档先写入临时 `GameState`，校验成功后再整体替换当前状态。
- 校验内容包括版本、单位定义、装备定义、重复 ID、位置冲突、装备归属等。
- 读档后会重建 Board/Bench 占用和 effective stats。

这是一个很好的设计点：失败读档不会污染当前游戏。

## 4. Catalog、EncounterGenerator、SkillContext

### Catalog

`src/core/Catalog.*` 是静态数据中心。

它定义：

- 可购买单位列表。
- 每个单位的 cost、traits、baseStats、factoryKey、visualKey。
- 装备列表。
- 根据 definition 创建对应派生类单位的 factory。

如果你要加新英雄，优先改这里，再看是否需要新增 `Unit` 派生类技能。

### EncounterGenerator

`src/core/EncounterGenerator.*` 负责 PvE 敌人生成。

它做得比较确定性：

- 根据回合数决定敌人数量，最多 3 个。
- 只在敌方半场找格子。
- 从中线附近开始找出生点。
- 敌人属性随回合和序号增长。

如果要做更复杂的关卡表，可以把这里替换成读取配置或脚本，但仍然通过 `GameState::addEnemyToBoard` 放入战场。

### SkillContext

`src/core/SkillContext.*` 是技能系统的受控访问层。

技能不直接拿整个 `GameState` 乱改，而是通过 `SkillContext`：

- 找目标位置。
- 找一行内敌人。
- 找半径内友军。
- 追加伤害事件。
- 追加治疗事件。

这样 `tickCombat()` 仍然统一控制伤害/治疗结算顺序。

## 5. 测试应该怎么读

把测试当作“代码契约”读。

`tests/test_stage1.cpp` 主要覆盖：

- 棋盘边界和半场规则。
- Bench 容量。
- Bench/Board/Unit placement 同步。
- Reject 和 Swap。
- 敌人不能进玩家 Bench。
- `Owner` 才是敌我来源。

`tests/test_stage2.cpp` 主要覆盖：

- 阶段守卫。
- Combat 中禁止布阵。
- 无上场玩家单位直接判负。
- 死敌人保留到结算。
- Bench 单位不参战。
- 敌人生成失败要原子回滚。
- 普攻回蓝不会同帧立即放技能。
- 索敌 tie-break。
- 移动冲突。
- 结算恢复玩家单位。
- GameOver 后拒绝继续行动。

`tests/test_stage3.cpp` 主要覆盖：

- 初始商店、购买、刷新。
- 经营 API 只能在 Prep。
- Combat 中拒绝存档。
- 人口限制和升级。
- 三合一升星。
- 装备返还。
- repeated recompute 不能重复叠 HP。
- 羁绊去重和战斗冻结。
- 装备属性影响。
- 读档失败不污染原状态。
- 存读档 round trip。

阅读测试时建议反向跳到实现：看到一个断言，就去找对应 `GameState` 函数。这样你会比从头顺读 `GameState.cpp` 更快掌控行为边界。

## 6. 你必须能讲清楚的核心不变量

如果 PA 要求你“完全掌控代码”，至少要能口头解释这些点：

1. 为什么 `Board` 和 `Bench` 只保存 `UnitId`，不保存 `Unit` 对象。
2. 为什么 `Board::setOccupant` 和 `Bench::setOccupant` 是 private。
3. 单位移动时哪三处状态必须同步。
4. `Owner` 和 `traits` 的区别。
5. `Prep`、`Combat`、`Resolve`、`GameOver` 各允许什么操作。
6. `activeEnemyUnits()` 和 `currentEnemyUnits()` 的区别。
7. `startCombat()` 为什么要保存玩家快照。
8. `tickCombat()` 为什么先收集事件再统一结算。
9. 为什么属性要从 `baseStats` 重算到 `effectiveStats`。
10. 羁绊为什么只统计 Board 上玩家单位，并按 `definitionId` 去重。
11. 读档为什么要先构造临时 `GameState`。
12. GUI 为什么不能直接修改 Board/Bench/UnitManager。

## 7. 推荐的动手熟悉路线

第一轮：只读结构。

- 画出 `GameState` 拥有哪些成员。
- 标出哪些成员是权威状态，哪些是运行时状态。
- 把 `Board`、`Bench`、`UnitManager` 的关系画成 ID 引用图。

第二轮：跑通阶段 1。

- 从 `test_stage1.cpp` 选一个测试，例如 `testBenchCapacityAndSync`。
- 手动跟踪：创建单位、进 Bench、部署到 Board、回 Bench。
- 每一步写下 Bench occupant、Board occupant、Unit placement 的变化。

第三轮：跑通一帧战斗。

- 从 `testBasicAttackManaDoesNotCastUntilNextReadyAttack` 入手。
- 跟踪 `startCombat()` 生成敌人和初始化冷却。
- 跟踪第一帧 `tickCombat()` 中普攻、回蓝、敌人扣血。
- 跟踪第二帧 Mana 已满后释放技能。

第四轮：跑通经营。

- 从 `testInitialShopPurchaseAndRefresh` 和 `testMergeKeepsNewestAndReturnsRemovedEquipment` 入手。
- 看购买如何创建单位。
- 看三合一保留谁、删除谁、装备怎么返还。

第五轮：跑通存档。

- 从 `testSaveLoadRoundTripRestoresAuthoritativeState` 入手。
- 看文件格式如何写出。
- 看读档如何校验。
- 特别注意失败读档为什么不会改变原对象。

第六轮：只读 GUI 边界，不深入美术和绘制细节。

- 看 `MainWindow` 如何连接按钮和回调。
- 看 `PlacementController` 如何把拖拽转换成 `GameState` API。
- 看 `BoardWidget` / `BenchWidget` 如何只读展示状态。

## 8. 修改核心逻辑时的安全原则

加新行为时优先问自己：

- 这个行为是否只应该在某个 phase 发生？
- 失败时是否会留下半更新状态？
- 是否需要更新 `Unit::placement`？
- 是否需要重算羁绊和 effective stats？
- 是否影响存档格式？
- 是否需要测试覆盖非法输入和失败分支？
- GUI 是否只需要调用新的 `GameState` API，而不是直接改 core 内部状态？

推荐新增功能方式：

- 新英雄：先加 `Catalog` 定义，再决定是否新增 `Unit` 派生类和技能。
- 新装备：先加 `ItemDefinition`，再扩展 `computeEffectiveStats`。
- 新羁绊：先扩展 `computeSynergiesFromBoard`，再扩展 `computeEffectiveStats` 或治疗/战斗逻辑。
- 新关卡：优先改 `EncounterGenerator`，不要让 GUI 负责生成敌人。
- 新存档字段：同时改 save、load、失败校验、round trip 测试。

## 9. GUI 代码单独指南

这一章独立于核心逻辑。后续你要替换 GUI 时，应保留 core 的接口思路，而不是复用 Qt 细节。

### 9.1 当前 GUI 总览

`src/gui/main_gui.cpp` 创建 `QApplication` 和 `MainWindow`。

`MainWindow` 拥有一个 `GameState game_`，所以当前 GUI 是本地单局游戏，不是外部注入状态。

当前界面组件：

- `BoardWidget`：画棋盘，处理棋盘单位选择和拖拽。
- `BenchWidget`：画备战区，处理 Bench 单位选择和拖拽。
- `ShopPanel`：显示商店 offer，触发购买和刷新。
- `EquipmentPanel`：显示装备库存，选择待穿戴装备。
- `SynergyPanel`：显示羁绊状态。
- `InspectorPanel`：显示选中单位详细属性。
- `PlacementController`：拖拽规则翻译层。
- `AssetManager`：按 `visualKey` 找图片，找不到则 widget 自己画色块。

### 9.2 GUI 和 core 的边界

当前 GUI 访问 core 的方式有两种：

- 只读展示：`const GameState*`，例如 BoardWidget、BenchWidget、InspectorPanel。
- 操作入口：`MainWindow` 或 `PlacementController` 调用 `GameState` API。

替换 GUI 时应该保留这个边界：

- 新 GUI 可以读 `GameState` 的公开查询接口。
- 新 GUI 的所有状态改变都应该通过 `GameState` 公开 API。
- 新 GUI 不应该直接写 `Board::setOccupant`、`Bench::setOccupant`、`UnitManager::remove`。

### 9.3 PlacementController 是替换 GUI 时最重要的参考

`PlacementController` 把拖拽来源和目标翻译成 core API：

- Bench -> 空 Board：`deployFromBench(..., Reject)`
- Bench -> 占用 Board：当前拒绝。
- Board -> 空 Board：`moveBoardUnit(..., Reject)`
- Board -> 己方占用 Board：`moveBoardUnit(..., Swap)`
- Board -> 空 Bench：`returnToBench`
- 目标 Bench 已占用：拒绝。

它还统一限制：

- 只有 `Prep` 阶段能拖动。
- 只有玩家单位能拖动。

后续换 GUI 时，可以保留这层思想：输入层负责解释鼠标/触摸/键盘操作，core 负责判断是否合法并执行。

### 9.4 BoardWidget 和 BenchWidget 只负责展示与输入

这两个 widget 做了很多绘制，但逻辑职责很窄：

- 从 `GameState` 读取 occupant。
- 根据 `UnitId` 查 `Unit`。
- 绘制格子、单位、HP/Mana 条、选中/悬停/拖拽覆盖层。
- 产生 `UnitDragData`。
- drop 时调用回调，不自己修改游戏状态。

如果要替换 GUI，你不需要复刻 QPainter 细节，只需要复刻这几个数据流：

```text
GameState -> read-only view -> user input -> PlacementController/GameState API -> refresh view
```

### 9.5 Shop、Equipment、Synergy、Inspector

这些 panel 都是薄展示层：

- `ShopPanel` 读 `shopOffers()`，点击按钮回调到 `MainWindow::purchaseShopSlot` 或 `refreshShop`。
- `EquipmentPanel` 读 `equipmentInventory()`，点击后记录 `selectedItem_`。
- `InspectorPanel` 读选中单位的所有公开属性。
- `SynergyPanel` 读 `activeSynergies()`。

替换 GUI 时，你只要实现同等交互：

- 商店购买和刷新调用 `GameState`。
- 装备栏先选装备，再点单位穿戴。
- Inspector 不改状态，只展示。
- 羁绊面板只读展示。

### 9.6 Combat Timer 是 GUI 层的帧驱动

`MainWindow` 用 `QTimer` 每 16 ms 调 `game_.tickCombat()`。

这说明 core 不是自己开线程或自己循环，战斗推进由外部驱动。换 GUI 时也是一样：

- Web GUI 可以用 requestAnimationFrame 或定时器。
- 控制台可以用循环。
- 测试可以手动调用 `tickCombat()`。

只要按 phase 调用 `startCombat()`、重复 `tickCombat()`、最后 `resolveRound()`，core 不关心 GUI 技术栈。

### 9.7 替换 GUI 时的最小接口清单

新 GUI 至少需要这些 core 查询：

- `phase()`
- `matchResult()`
- `player()`
- `board()`
- `bench()`
- `unit(UnitId)`
- `shopOffers()`
- `equipmentInventory()`
- `activeSynergies()`
- `deployedPlayerUnitCount()`

新 GUI 至少需要这些 core 操作：

- `deployFromBench`
- `moveBoardUnit`
- `returnToBench`
- `purchaseShopSlot`
- `refreshShop`
- `upgradePopulation`
- `equipItem`
- `saveToFile`
- `loadFromFile`
- `startCombat`
- `tickCombat`
- `resolveRound`

把这些 API 包成你新 GUI 的 ViewModel/Controller，就可以替换掉 Qt。

## 10. 构建与验证

项目的 CMake 目标包括：

- `synera_core`
- `synera_cli`
- `stage1_tests`
- `stage2_tests`
- `stage3_tests`
- `synera_gui`

已有 README 给出了 Windows/PowerShell 下的构建命令。熟悉代码时，建议每次核心改动后至少运行：

```powershell
.\build\stage1_tests.exe
.\build\stage2_tests.exe
.\build\stage3_tests.exe
```

测试程序无输出通常表示断言全部通过。

如果只改 GUI，至少要手动验证：

- 可以选中 Board/Bench 单位。
- Prep 阶段能拖拽，Combat 阶段不能拖拽。
- 商店按钮在 Prep 可用，Combat 禁用。
- Start Combat 后 timer 推进战斗。
- Resolve 后能回到 Prep 或 GameOver。

## 11. 最后一条主线

你可以把整个项目记成一句话：

`GameState` 是唯一会改变游戏事实的地方；`UnitManager` 拥有单位；`Board`/`Bench` 只保存 ID；`Unit` 保存自身属性和 placement；战斗由外部逐帧驱动；GUI 只是读状态、发命令。

只要这句话能解释得通，你就抓住了这份代码的骨架。
