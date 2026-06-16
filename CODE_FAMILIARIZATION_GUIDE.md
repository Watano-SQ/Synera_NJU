# Synera / PvZ Auto Arena 代码熟悉指南

这份指南面向“全面掌控当前代码”。它不只是 README 的展开版，而是把当前实现拆成可检查的规则、数据结构、流程和测试锚点。项目名、命名空间和 CMake target 仍叫 Synera；当前题材和素材是 PvZ 自走棋。

## 先记住三条边界

1. 行为以 `src/core/` 和 `tests/` 为准，README 只负责入口介绍。
2. `GameState` 是核心状态和规则入口；`Board`、`Bench`、`UnitManager` 只做低层存储；GUI 不应该直接拥有游戏规则。
3. 当前若干源码字符串里存在真实 mojibake，尤其是 `Catalog.cpp`、`EncounterGenerator.cpp`、`src/main.cpp`、部分 GUI 文案和 `tools/build_assets.py` 的中文显示名。逻辑判断不要依赖这些显示名，要依赖 `definitionId`、`factoryKey`、`traitId`、`visualKey`、`itemDefId` 这些稳定 ASCII ID。

PA 说明文档给的是目标和 checklist：棋盘、Bench、单位属性、owner/traits、Prep/Combat/Resolve、GUI、商店、羁绊、三合一、装备、存档、扩展方向等。当前 PDF 的文本抽取会因为字体映射乱码，所以读规则时优先看 PDF 原文视觉版，落到代码时再用源码和测试验证。

## 推荐阅读顺序

1. `CMakeLists.txt`
   看 target：`synera_core`、`synera_cli`、`stage1_tests`、`stage2_tests`、`stage3_tests`、`synera_gui`、`gui_layout_tests`。

2. `src/core/Types.h`
   先掌握全局类型：`Owner`、`UnitState`、`GamePhase`、`RoundResult`、`MatchResult`、`PlacementKind`、`PlacementPolicy`、`BoardHalf`、`ItemEffectType`，以及 `Position`、`ActionResult`、`UnitStats`、`UnitDefinition`、`ShopOffer`、`ItemDefinition`、`TraitDefinition`、`ItemInstance`、`SynergyStatus`、`CombatConfig`、`Placement`。

3. `src/core/Board.*`、`Bench.*`、`Player.*`、`UnitManager.*`
   这些是状态容器。重点看它们“不做什么”：不做战斗、不做商店、不做存档、不主动改规则。

4. `src/core/Unit.*`、`SkillContext.*`
   理解单位实例、技能多态、战斗效果怎样被延迟收集。

5. `src/core/Catalog.*`、`EncounterGenerator.*`
   理解可购买单位、显示资源、装备、羁绊、敌人模板和刷怪位置。

6. `src/core/GameState.*`
   这是全项目最重要的文件：商店、部署、移动、升星、装备、羁绊、战斗、结算、存档都在这里闭环。

7. `tests/`
   用四个测试文件反查边界条件。测试是当前行为最可靠的验收文档。

8. `src/gui/`
   GUI 单独读。它是现在的表现层，但以后可以替换，所以不要把 GUI 当成规则来源。

9. `tools/build_assets.py`、`assets/`、`pictures/`
   最后看资源管线，确认 `visualKey` 到文件的映射。

## Core 总览

### CMake targets

- `synera_core`：所有核心规则，供 CLI、测试、GUI 复用。
- `synera_cli`：简单命令行演示，主要用于早期状态打印，不是完整游戏入口。
- `stage1_tests`：棋盘、半场、Bench、部署、交换、敌人生成、敌人不能进 Bench。
- `stage2_tests`：战斗状态机、阶段守卫、刷怪原子性、技能时机、寻路/移动冲突、结算恢复、GameOver 守卫。
- `stage3_tests`：商店、人口、合成、装备、羁绊、资产 key、存档读写和失败不污染原状态。
- `synera_gui`：Qt Widgets GUI。
- `gui_layout_tests`：GUI 固定尺寸和截图 smoke test。

### Types.h

- `Owner::PlayerCtrl` / `Owner::EnemyCtrl` 是玩家和敌人的根本区分。
- `UnitState` 用于战斗动画/状态表达：`Idle`、`Moving`、`Attacking`、`Casting`、`Dead`。
- `GamePhase` 是状态机：`Prep` -> `Combat` -> `Resolve` -> `Prep`，最终到 `GameOver`。
- `PlacementKind` 把单位位置分成 `None`、`BenchSlot`、`BoardCell`。
- `PlacementPolicy::Reject` / `Swap` 控制占位时拒绝或交换。
- `CombatConfig` 是战斗参数入口：攻击间隔、移动间隔、攻击回蓝、胜败金币、败北扣血、最大轮次、装备掉落概率。
- `ActionResult` 用于返回成功与消息。当前有些消息字符串 mojibake，但 `success` 是逻辑依据。

### Board

`Board` 是一个二维格子的 `UnitId` 存储层。

- 默认 8x8。
- `isInside` 判断边界。
- `isEnemyHalf` / `isPlayerHalf` 用行号切半：上半敌方，下半玩家。
- `isEmpty` 和 `occupant` 只看格子是否有 `UnitId`。
- `canPlace` 只检查格子在内、为空、并符合 owner 所属半场。
- `setOccupant` / `clear` 是 private，只有 `GameState` friend 可以改。

这意味着外部代码不能绕过 `GameState` 直接写棋盘。

### Bench

`Bench` 是一维备战区 `UnitId` 存储层。

- 默认容量 8。
- `occupant(std::size_t)` 和 `occupant(int)` 都存在，后者会把负数/越界转成 `nullopt`。
- `firstEmptySlot` 用于购买和添加单位。
- `setOccupant` / `clear` 同样 private，只让 `GameState` 改。

敌人不能进入 Bench，这条由 `GameState::addUnitToBench` 和 `returnToBench` 守住。

### Player

`Player` 只保存玩家数值：

- `hp`
- `gold`
- `level`
- `unitCap`
- `currentRound`

升级人口、胜败结算、当前波次推进都由 `GameState` 调它的 setter。

### UnitManager

`UnitManager` 是单位对象所有权容器。

- 内部是 `unordered_map<UnitId, unique_ptr<Unit>>`。
- `add` 自动分配递增 `UnitId`。
- `addWithId` 用于读档恢复原 ID。
- `nextId` / `setNextId` 保证存档后的 ID 连续。
- `remove` 真正销毁单位对象。

`Board` 和 `Bench` 只放 ID，真正的 `Unit` 对象都在这里。

### Unit

`Unit` 是玩家植物和敌人僵尸共用的基类。

核心字段：

- `definitionId`：逻辑身份，升星后不变。
- `name`：显示名，当前部分源码已 mojibake，不要作为逻辑依据。
- `visualKey`：基础显示资源 key。
- `factoryKey`：决定创建哪种派生类。
- `owner`：玩家或敌人。
- `traits`：羁绊标签。
- `star`：当前只支持 1 或 2。
- `acquireSeq`：购买/添加顺序，用于三合一保留最新单位。
- `baseStats`：基础属性。
- `effectiveStats`：星级、装备、羁绊结算后的属性。
- `equippedItemId`：当前装备，最多一件。
- `state`：战斗状态。
- `placement`：当前位置。

属性读法要注意：`maxHp()`、`atk()`、`range()`、`maxMana()`、`attackInterval()`、`moveInterval()` 都来自 `effectiveStats`，不是 `baseStats`。

派生类：

- `BasicUnit`：无特殊技能。
- `PeaBurst`：技能对当前目标造成 `atk() * 2`。
- `FumeLineCaster`：技能打目标所在行的所有敌方单位。
- `SunHealer`：技能治疗以自己为中心半径 2 内的友军，治疗量是自身 `atk()`。

### SkillContext

技能不能直接到处改 `GameState`，而是通过 `SkillContext` 追加效果：

- `dealDamage` 追加 `CombatEffect` 到伤害队列。
- `heal` 追加 `CombatEffect` 到治疗队列。
- `findUnitsInLine` 按行找指定 owner 的存活单位。
- `findAlliesInRadius` 按欧氏距离平方找同 owner 存活单位。

`GameState::tickCombat` 最后统一 `applyCombatEffects`，所以同一 tick 内的伤害/治疗有明确顺序。

## Catalog 与资源 key

`Catalog.cpp` 定义三类目录：单位、装备、羁绊。

### 可购买单位

`unitCatalog()` 当前只有 6 个基础植物：

| definitionId | cost | traits | baseStats | factoryKey | star1VisualKey | star2VisualKey |
| --- | ---: | --- | --- | --- | --- | --- |
| `peashooter` | 1 | `shooter` | HP 300, ATK 32, Range 3, Mana 60, AtkInt 60, MoveInt 20 | `BasicUnit` | `units/peashooter` | `units/repeater` |
| `sunflower` | 2 | `sun`, `healer` | HP 260, ATK 22, Range 2, Mana 60, AtkInt 64, MoveInt 20 | `SunHealer` | `units/sunflower` | `units/twin_sunflower` |
| `wallnut` | 2 | `nut` | HP 520, ATK 18, Range 1, Mana 70, AtkInt 70, MoveInt 20 | `BasicUnit` | `units/wallnut` | `units/tallnut` |
| `puffshroom` | 1 | `fungus` | HP 220, ATK 24, Range 2, Mana 45, AtkInt 48, MoveInt 20 | `BasicUnit` | `units/puffshroom` | `units/scaredyshroom` |
| `fumeshroom` | 2 | `fungus`, `shooter` | HP 320, ATK 30, Range 2, Mana 60, AtkInt 60, MoveInt 20 | `FumeLineCaster` | `units/fumeshroom` | `units/gloomshroom` |
| `spikeweed` | 2 | `spike` | HP 300, ATK 34, Range 1, Mana 60, AtkInt 50, MoveInt 20 | `BasicUnit` | `units/spikeweed` | `units/spikerock` |

`repeater`、`twin_sunflower`、`tallnut`、`scaredyshroom`、`gloomshroom`、`spikerock` 只是二星视觉资源，不在 `unitCatalog()`，也不应该出现在商店 offer。

`displayVisualKey(const Unit&)` 的规则：

- 敌人直接用自身 `visualKey()`。
- 玩家二星优先用目录里的 `star2VisualKey`。
- 玩家一星优先用 `star1VisualKey`。
- 找不到定义时回退到单位自身 `visualKey()`。

### 装备目录

`itemCatalog()` 当前有 4 件装备：

- `plant_food`：`items/plant_food`，`Attack +15`。
- `pumpkin_shell`：`items/pumpkin_shell`，`MaxHp +150`。
- `garden_glove`：`items/garden_glove`，`AttackIntervalPercent -20`，也就是攻击间隔变短。
- `chlorophyll`：`items/chlorophyll`，`MaxMana -30`，但 `computeEffectiveStats` 会保证最大法力不低于 10。

每个单位最多装备一件。装备从库存移到单位身上；合成时被删除单位的装备会回到库存。

### 羁绊目录与实际效果

`traitCatalog()` 只给显示数据；真正阈值和效果在 `GameState::computeSynergiesFromBoard()` 与 `computeEffectiveStats()`。

当前羁绊按“场上玩家单位的不同 `definitionId`”计数，不按单位数量计数。因此两个 `peashooter` 只贡献 1 个 `shooter`。

实际阈值：

- `shooter`：2/4，攻击间隔 -10% / -20%。
- `nut`：2/4，最大生命 +200 / +450。
- `sun`：2/3，胜利结算阳光 +1 / +2。
- `healer`：2/2，治疗效果 +25%。
- `fungus`：2/4，最大法力 -10 / -25，最低仍为 10。
- `spike`：2/2，攻击力 +15。

`activeSynergies_` 在战斗开始时冻结；战斗中单位死亡不会立刻改变羁绊状态。

## EncounterGenerator

敌人生成分两步：先 `plan`，再 `generate`。

### 敌人模板

- Round 1：普通僵尸。
- Round 2：普通僵尸、路障僵尸。
- Round 3：普通僵尸、路障僵尸、铁门僵尸。
- Round 4：读报僵尸、路障僵尸、铁门僵尸。
- Round 5+：橄榄球僵尸、小丑僵尸、铁门僵尸。

每过一轮，敌人基础模板会按 `round - 1` 增加生命和攻击：

- `maxHp += scalingRounds * 25`
- `atk += scalingRounds * 4`

敌人都是 `BasicUnit`，owner 是 `EnemyCtrl`，trait 主要是 `zombie` 以及 `armored`、`frenzy`、`elite` 等。

### 位置选择

刷怪只用敌方半场。列顺序从中心向两侧扩展：center、left、right，再继续外扩。`startCombat()` 会先用 `ignoreExistingEnemies = true` 预规划，确认可生成后清掉旧敌人再真正加入，避免半生成状态污染棋盘。

## GameState 是全局规则入口

`GameState` 拥有这些核心状态：

- `Player player_`
- `Board board_`
- `Bench bench_`
- `UnitManager units_`
- `GamePhase phase_`
- `RoundResult lastRoundResult_`
- `MatchResult matchResult_`
- `CombatConfig combatConfig_`
- `shopOffers_`
- `itemInstances_`
- `equipmentInventory_`
- `activeSynergies_`
- `nextItemId_`
- `nextAcquireSeq_`
- `rng_`
- `currentEnemyUnits_`
- `playerCombatSnapshot_`
- `runtime_`

构造函数会创建棋盘、Bench、战斗配置，并立即 `generateShopOffersFree()` 生成 5 个商店格。

### 公开读写 API

读状态：

- `player()`、`board()`、`bench()`、`units()` 有 const 和非 const 版本。
- `phase()`、`lastRoundResult()`、`matchResult()`、`combatConfig()`。
- `shopOffers()`。
- `equipmentInventory()` 返回库存 item 的值拷贝。
- `item(ItemId)` 查装备实例。
- `activeSynergies()`。
- `deployedPlayerUnitCount()`。
- `unit(UnitId)` 查单位。

配置：

- `setCombatConfig(CombatConfig)`。
- `setItemDropPercent(int)`，内部 clamp 到 0-100。

单位/装备：

- `addUnitToBench(unique_ptr<Unit>)`：只允许玩家单位，Bench 满会抛异常。
- `addItemToInventory(itemDefId)`：未知装备定义会抛异常。
- `equipItem(itemId, unitId)`：只允许 Prep、库存中装备、玩家单位、单位未装备。
- `computeEffectiveStats(unitId)`：从 base 重新计算，不叠脏状态。

商店/人口：

- `refreshShop()`：只允许 Prep，花 2 金，重新生成 5 个 offer。
- `purchaseShopSlot(index)`：只允许 Prep，检查索引、offer、金币、Bench 空位；购买后 add 到 Bench、尝试合成、清空该 offer。
- `upgradePopulation()`：只允许 Prep；最高 level 6，unitCap 最高 8；费用是 `4 + 2 * (level - 1)`。

部署：

- `deployFromBench(slot, target, policy)`：只允许 Prep；检查 Bench、棋盘边界、半场、人口上限和占位策略。
- `moveBoardUnit(from, to, policy)`：只允许 Prep；同格移动视为成功；可空格移动或同 owner 交换。
- `returnToBench(from, slot)`：只允许 Prep；只允许玩家单位回到空 Bench。

敌人/战斗：

- `generateEnemiesForRound(round)`：清掉旧敌人，设置当前轮次，直接生成敌人。主要用于测试/演示。
- `startCombat()`：Prep -> Combat 或直接 Resolve。
- `tickCombat()`：Combat 内推进一帧。
- `resolveRound()`：Resolve -> Prep/GameOver。
- `activePlayerUnits()`、`activeEnemyUnits()`、`activeCombatUnits()` 只返回在棋盘上且存活的单位。
- `currentEnemyUnits()` 返回本波生成过的敌人 ID；死了但未结算的敌人仍保留在这个列表中。

## Prep 阶段规则

Prep 是唯一允许玩家管理阵容的阶段。

允许：

- 刷新商店。
- 购买商店单位。
- 升级人口。
- 穿戴装备。
- 从 Bench 部署到玩家半场。
- 移动玩家半场单位。
- 交换同 owner 棋盘单位。
- 从棋盘回 Bench。
- 存档。
- 读档。

不允许：

- 把玩家单位放到敌方半场。
- 把敌方单位放进 Bench。
- 超过人口上限部署玩家单位。
- 给非玩家单位穿装备。
- 给已有装备的单位再穿一件。

每次 Prep 中影响阵容/装备的动作都会触发 `recomputePrepSynergiesAndStats()`。这个函数只在 `Prep`、`Resolve`、`GameOver` 里重算羁绊和玩家有效属性，不在 Combat 中随死亡实时变。

## 三合一升星

合成入口是 `maybeMergeUnit(newestId)`，由 `addUnitToBench` 和 `purchaseShopSlot` 间接触发。

规则：

- 只在 Prep 生效。
- 只处理玩家单位。
- 只处理一星单位。
- 要求同 `definitionId` 且在 Bench 或 Board 上的单位数量至少 3。
- 按 `acquireSeq` 从新到旧排序，保留最新的那一个。
- 保留单位 `star = 2`。
- 被删除单位从 Bench/Board 清除，并从 `UnitManager` 移除。
- 被删除单位身上的装备回到库存。
- 保留单位的 `definitionId`、`traits`、`factoryKey`、技能类不变。
- 二星只影响属性倍率和显示贴图：`maxHp`、`atk` 乘 1.7 并四舍五入。

测试里特别验证了：二星 `spikeweed` 仍然是 `spikeweed`，`factoryKey` 仍是 `BasicUnit`，显示才变成 `units/spikerock`。

## 属性重算

`computeEffectiveStats(UnitId)` 的顺序很重要：

1. 从 `baseStats` 拷贝。
2. 如果二星，`maxHp` 和 `atk` 乘 1.7。
3. 应用装备：
   - Attack 直接加攻击。
   - MaxHp 直接加生命。
   - MaxMana 加到最大法力，最低 10。
   - AttackIntervalPercent 用百分比调整攻击间隔。
4. 应用激活羁绊：
   - `shooter` 降低攻击间隔。
   - `nut` 增加最大生命。
   - `fungus` 降低最大法力，最低 10。
   - `spike` 增加攻击。
5. 写回 `effectiveStats`，并按新上限 clamp 当前 `hp` 和 `mana`。

这个设计避免“重复重算时属性越叠越高”的 bug。

## Combat 阶段规则

### startCombat()

`startCombat()` 的守卫：

- 只能从 Prep 调用。
- `matchResult_` 必须是 `Ongoing`。

开始时做这些事：

1. 计算并冻结当前羁绊。
2. 重算玩家有效属性。
3. 找棋盘上的存活玩家单位。
4. 如果没有上场玩家单位：清敌人、清快照/运行时、直接进入 Resolve，并记为玩家失败。
5. 预规划敌人生成。如果没有可用生成格，返回失败并保持 Prep。
6. 清旧敌人。
7. 对当前上场玩家单位记录 `playerCombatSnapshot_`。
8. 生成本波敌人并写入 `currentEnemyUnits_`。
9. 切到 Combat，清 round result，初始化 runtime。

这保证刷怪失败是原子失败，不会出现半个战斗状态。

### tickCombat()

`tickCombat()` 只能在 Combat 调用。每 tick 做：

1. 获取所有存活 combat units。
2. 对每个 active unit 更新攻击/移动冷却。
3. 为每个单位选择目标。
4. 若目标在攻击范围：
   - 攻击冷却没好就 Idle。
   - 冷却好后，如果 mana 已满，进入 Casting，调用 `castSkill`，mana 清零。
   - 否则普通攻击，造成 `atk()` 伤害，并通过 `grantsMana` 回蓝。
5. 若不在攻击范围：
   - 移动冷却没好就 Idle。
   - 否则通过 BFS 找下一步，加入移动提案，状态设为 Moving。
6. 统一结算伤害/治疗。
7. 清除死者的棋盘占位。
8. 统一应用移动提案。
9. 检查是否进入 Resolve。

攻击距离用欧氏距离平方：`dr * dr + dc * dc <= range * range`。

### 目标选择

`selectTarget` 的优先级：

1. 距离平方更小。
2. 距离相同，当前 HP 更高。
3. 仍相同，列号更小。
4. 仍相同，行号更大。
5. 仍相同，`UnitId` 更小。

测试覆盖了“HP 相同优先更小列”的 tie-break。

### 移动与冲突

`nextStepTowardAttackRange` 使用 BFS：

- 方向顺序：上、左、右、下。
- 只能走棋盘内空格。
- 目标是走到攻击范围内，而不是走到目标所在格。
- 如果第一步就达成目标，直接返回第一步。
- 否则返回到达目标范围的路径的第一步。

`applyMoveProposals` 会先统计每个目标格的提案数量。多个单位争同一格时，该格谁都不进，保持空/原状，避免顺序依赖。

### 伤害、治疗、死亡

`applyCombatEffects` 先处理伤害，再处理治疗。

- 普通攻击的 `CombatEffect` 会 `grantsMana = true`，让攻击者获得 `combatConfig_.manaPerAttack`。
- 技能造成的 `dealDamage` 不回蓝。
- healer 羁绊激活时，治疗量提升 25%。
- `clearDeadBoardOccupants` 会把死亡单位状态设为 `Dead`，placement 设为 `None`，并清棋盘格。

### resolveRound()

`resolveRound()` 只能在 Resolve 调用。

共有步骤：

1. 清掉生成敌人。
2. 从 `playerCombatSnapshot_` 恢复玩家棋盘位置。
3. 玩家单位恢复满血、mana 归零、state 归 `Idle`。
4. 清 runtime 和快照。

胜利：

- 玩家获得 `victoryGold + sun羁绊加成`。
- 按 `itemDropPercent` 可能掉装备。
- 当前轮次达到 `maxRound` 时进入 GameOver 且 `matchResult = PlayerVictory`。
- 否则轮次 +1，回 Prep。

失败：

- 玩家获得 `defeatGold`。
- 玩家 HP 减 `defeatHpLoss`。
- HP 小于等于 0 时进入 GameOver 且 `matchResult = PlayerDefeat`。
- 否则不推进轮次，回 Prep 重打当前波。

`currentEnemyUnits()` 在 Resolve 前仍保留死敌 ID；`resolveRound()` 后才清空并删除敌人。

## 存档系统

`saveToFile(path)`：

- Combat 阶段禁止存档。
- 写 `SYNERA_SAVE 1`。
- 保存 Board 尺寸、Bench 容量、Player、phase/result、next IDs、shop、items、玩家单位、snapshots。
- 只保存玩家单位，不保存当前生成敌人。
- 单位保存 `id`、`definitionId`、owner、star、acquireSeq、hp、mana、equippedItemId、placement。

`loadFromFile(path)`：

- 先读到一个临时 `GameState temp`。
- 校验版本、棋盘、Bench、Player、phase、shop、items、units、placement、snapshots、END。
- 拒绝 Combat 存档。
- 拒绝未知 `definitionId` / `itemDefId`。
- 拒绝重复 UnitId、重复 itemId、冲突 Bench/Board 位置、非法半场、装备归属不一致。
- `rebuildOccupancyFromUnitPlacements()` 后再重算羁绊和属性。
- pending vitals 最后恢复 HP/Mana。
- 全部成功后才 `*this = std::move(temp)`。

因此读档失败不会污染当前状态，`stage3_tests` 专门覆盖这一点。

## GUI 分层

GUI 在 `src/gui/`，目前是 Qt Widgets。理解时要和 core 分开：

- `MainWindow` 持有唯一的 `GameState game_`。
- `BoardWidget`、`BenchWidget`、`ShopPanel`、`EquipmentPanel`、`SynergyPanel`、`InspectorPanel` 都读 `const GameState*`。
- 拖拽操作传 `UnitDragData` 值对象，不传 raw pointer。
- 放置规则由 `PlacementController` 调 `GameState`，不是 widget 自己改棋盘。
- 商店、装备、存档、战斗按钮由 `MainWindow` 调 `GameState` API。
- `refreshFromState()` 是 GUI 同步入口。

### MainWindow

布局：

- 顶部状态条：脑子/HP、旗帜波次、阶段、等级、人口、升级/存档/读档/开始/结算按钮。
- 左列固定宽度：`BoardWidget` 512x512，`BenchWidget` 512x96。
- 右列固定宽度：商店、装备、羁绊滚动区、Inspector 滚动区。
- 最小窗口 1360x760。

初始化：

- 初始 gold = 8，level = 1，unitCap = 3。
- 添加 peashooter、sunflower、wallnut。
- peashooter 自动部署到 `{7, 3}`。

战斗：

- `combatTimer_` 间隔 16 ms。
- 开始战斗成功且 phase 是 Combat 时启动 timer。
- timer 每次调 `tickCombat()`。
- 进入 Resolve 或 tick 失败时停止。

状态按钮：

- 只有 Prep 且 match ongoing 才能 Start Combat。
- 只有 Resolve 才能 Resolve。
- Prep 才允许拖拽、升级、管理。
- Combat 禁止保存/读取。

### BoardWidget

职责：

- 画棋盘背景、格线、单位、血蓝条、选中、hover、drop overlay。
- 上半场用 `boardHalfBackgroundVisualKey(BoardHalf::Enemy)`，下半场用 Player。
- 固定格子 64px，所以 8x8 是 512x512。
- 鼠标点击选中单位或清空选择。
- Prep 中玩家单位可拖拽。
- 拖拽 payload 是 `UnitDragData` 编码进 `application/x-synera-unit-drag`。

绘制顺序很重要：背景 -> grid -> unit -> selection -> hover -> drop。这样 hover/drop 只作为覆盖层，不改变基础格子颜色。

### BenchWidget

职责类似棋盘，但一维：

- 固定 512x96。
- slot 尺寸 58x64，间距 5，左 padding 6，上 padding 14。
- 画 Bench 背景、slot base、grid、单位、选中、hover、drop。
- 支持从 Bench 拖到 Board、从 Board 拖回 Bench。
- 当前不支持 Bench 内重排。

### PlacementController

这是 GUI 的放置规则边界。

- `canDrag(id)`：只有 Prep 且玩家单位可拖。
- `dropOnBoard`：
  - Bench -> Board：当前只支持目标空格，调用 `deployFromBench(..., Reject)`。
  - Board -> Board：空格移动；若目标是玩家单位则 `Swap`；敌人/非法目标拒绝。
- `dropOnBench`：
  - 只支持 Board -> 空 Bench。
  - Bench -> Bench 重排当前明确未实现。

注意：core 支持 Bench 到占用 Board 的 swap，但 GUI 当前故意没开放这条交互。

### DragData

`UnitDragData` 字段：

- `unitId`
- `sourceType`：Bench 或 Board
- `benchSlot`
- `boardPosition`

编码/解码用 `QByteArray + QDataStream`，这是为了避免 GUI 传对象指针。

### ShopPanel

职责：

- 画 5 个商店卡。
- 画阳光/金币计数。
- 画刷新按钮。
- 点击卡调用 purchase callback。
- 点击刷新调用 refresh callback。
- 判断是否可购买时看 `shopOffers`、金币和空 offer。

商店卡资源来自 `assets/shop_cards/<definitionId>_available.png` 和 `_disabled.png`，只对应基础可购买植物。

### EquipmentPanel

职责：

- 展示 `equipmentInventory()`。
- 选中一个 item。
- `MainWindow::setSelectedUnit` 会尝试把选中的装备穿到被点中的单位上。
- 使用 `EquipmentTrayWidget` 和 `ui/zombieline` / `ui/zombieline_tray` 做横向托盘。

### SynergyPanel

职责：

- 读取 `activeSynergies()`。
- 展示每个 trait 的 count、阈值和效果状态。
- `activeSynergies_` 的内容来自 core，GUI 不自行计算羁绊。

### InspectorPanel

职责：

- 展示选中单位详情：名字、星级、装备、archetype、owner、state、HP、ATK、range、mana、base/effective stats、traits、placement、visualKey。
- 这是调试当前状态非常重要的面板。

### AssetManager 和 PaintUtils

`AssetManager` 搜索顺序：

1. 可执行文件旁边的 `assets`。
2. 编译时 `SYNERA_PROJECT_ROOT/assets`。
3. 当前工作目录的 `assets`。

`pixmapFor(visualKey)` 会尝试：

- 原 key。
- `key + ".png"`。
- `key + ".jpg"`。
- `key + ".jpeg"`。

`PaintUtils` 提供等比适配和裁剪填充，避免各 widget 重复写图片缩放逻辑。

## 资源管线

`assets/` 是运行时资源，`pictures/` 是源素材池，`tools/build_assets.py` 负责生成。

生成内容：

- `assets/units/`：基础植物和二星视觉，包括 repeater、twin_sunflower、tallnut、scaredyshroom、gloomshroom、spikerock。
- `assets/enemies/`：zombie、conehead、screendoor、newspaper、football、jackbox。
- `assets/items/`：plant_food、pumpkin_shell、garden_glove、chlorophyll。
- `assets/traits/`：6 个 trait badge。
- `assets/shop_cards/`：6 个基础植物的 available/disabled 种子包卡。
- `assets/ui/`：button、brain、sun_counter、flag meter、plant_shop、zombieline、zombieline_tray。
- `assets/backgrounds/`：day_board、night_board。

`stage3_tests` 会检查 catalog 里每个 visual key 都能在 assets 下找到对应图片。

## 测试怎么读

### stage1_tests

覆盖基础棋盘和放置：

- 8x8 board。
- player half / enemy half。
- 玩家单位不能放敌方半场。
- Bench 容量和越界 occupant。
- Bench -> Board -> Bench 的 placement 同步。
- Reject 和 Swap placement。
- 敌人生成在敌方半场。
- `activePlayerUnits` / `activeEnemyUnits` 只含上场单位。
- 敌人不能进入玩家 Bench。

### stage2_tests

覆盖战斗：

- 非 Combat 不能 tick/resolve。
- Combat 中不能部署、移动、回 Bench。
- 无上场玩家单位会直接失败进入 Resolve，不推进轮次。
- `currentEnemyUnits` 在死敌进入 Resolve 后仍保留，resolve 后清空。
- Bench 单位不进入 active combat list。
- 刷怪失败保持原状态。
- 普攻先回满 mana，下一次 ready attack 才施法。
- 目标选择 tie-break。
- 多单位移动冲突不会抢占同一格。
- Resolve 后恢复玩家位置、HP、mana、state、Bench。
- GameOver 后拒绝继续 start/tick/resolve/move。

### stage3_tests

覆盖经济、装备、羁绊、存档、资产：

- 初始商店 5 个 offer，且不含二星视觉 ID。
- 购买扣钱、清 offer、进 Bench。
- 刷新花 2 金。
- Combat 中拒绝商店、人口、装备、存档。
- 人口上限和升级后部署。
- 三合一保留最新单位，删除单位装备回库存。
- 重复 `computeEffectiveStats` 不重复叠 HP delta。
- 羁绊按 `definitionId` 去重。
- 战斗中羁绊冻结。
- 装备分别影响攻击、最大生命、最大法力、攻击间隔。
- sun 羁绊只给胜利结算加钱，失败不加。
- catalog 和 asset key 全部存在。
- 读坏档失败不污染原状态。
- 存档 round trip 恢复 board、bench、player、装备和属性。

### gui_layout_tests

覆盖 GUI 几何和截图：

- Board 固定 512x512。
- Bench 固定宽 512、高 96。
- Shop 高度受限、宽度能容纳合并商店容器。
- Equipment panel 和 tray 尺寸受限。
- MainWindow 最小 1360x760。
- 会保存 `artifacts/gui_layout_grab.png` 作为布局 smoke screenshot。

## 当前实现的注意点

- 部分中文显示字符串已经真实 mojibake，不只是 PowerShell 显示问题。当前逻辑依赖稳定 ID，所以不影响测试中的核心规则，但 GUI 文案和显示名会受影响。
- `synera_cli` 是演示程序，不是权威交互入口；GUI 和测试覆盖更多当前功能。
- GUI 当前不支持 Bench 内重排，也不支持 Bench -> occupied Board 的 swap。
- `GameState::generateEnemiesForRound` 会直接清旧敌人并生成新敌人，适合测试/演示；正式战斗入口应走 `startCombat`。
- 存档不保存当前敌人，只保存玩家状态、商店、装备、单位、快照等可恢复状态。
- `activeSynergies_` 是 core 状态，不是 GUI 临时计算；Combat 中故意冻结。
- `currentEnemyUnits_` 和 `activeEnemyUnits()` 语义不同：前者是本波生成过的敌人，后者是当前棋盘上存活敌人。

## 修改代码时的安全顺序

1. 先在 `PA说明文档.pdf` 找对应 checklist 或规则动机。
2. 在 `tests/` 找已有验收点。
3. 在 `GameState` 加或改规则，不要只在 GUI 禁用按钮。
4. 低层容器仍保持简单：`Board`/`Bench` 存 ID，`UnitManager` 管对象。
5. 如果改属性，确保从 `baseStats` 重算，不从旧 `effectiveStats` 叠加。
6. 如果改战斗，优先补 `stage2_tests`。
7. 如果改商店、装备、羁绊、存档、资产，优先补 `stage3_tests`。
8. 如果改 GUI 布局或绘制，跑 `gui_layout_tests`，并检查截图。
9. 如果改拖拽，保持 `UnitDragData` 值传递和 `PlacementController` 边界。
10. 最后再跑相关 target。

## 常用验证命令

```powershell
cmake --build build --config Debug
.\build\stage1_tests.exe
.\build\stage2_tests.exe
.\build\stage3_tests.exe
```

GUI layout test:

```powershell
$env:QT_QPA_PLATFORM = "offscreen"
.\build\gui_layout_tests.exe
Remove-Item Env:\QT_QPA_PLATFORM
```

重新生成资产：

```powershell
python .\tools\build_assets.py
```

运行 GUI：

```powershell
.\build\synera_gui.exe
```

如果测试可执行文件成功时没有输出，这是正常的；这些测试主要靠 `assert` 失败中断。
