#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace synera {

// 全局 ID 只作为 UnitManager 和装备表里的稳定句柄使用。
// 棋盘、备战区、GUI 都通过 ID 间接访问对象，避免复制 Unit。
using UnitId = std::uint64_t;
using ItemId = std::uint64_t;

// 单位归属。很多规则都依赖归属判断，例如半场限制、目标选择和羁绊加成。
enum class Owner {
    // 玩家可购买、部署、装备和参与羁绊统计的单位。
    PlayerCtrl,
    // 由 EncounterGenerator 生成，只在战斗与结算过程中短暂存在的敌方单位。
    EnemyCtrl
};

// 战斗表现状态。它既用于逻辑判断 Dead，也供 GUI 展示当前动作。
enum class UnitState {
    Idle,
    Moving,
    Attacking,
    Casting,
    Dead
};

// 游戏主状态机。大多数会改动阵容、商店或装备的命令只允许在 Prep 阶段执行。
enum class GamePhase {
    // 准备阶段允许经济、商店、装备、部署和存读档操作。
    Prep,
    // 战斗阶段只允许 tickCombat 推进，禁止玩家再改阵容。
    Combat,
    // 结算阶段根据胜负发放奖励、扣血、恢复玩家阵容。
    Resolve,
    // 游戏结束后保留最终状态供 GUI 展示，不再继续推进回合。
    GameOver
};

// 单回合结算结果，Resolve 阶段会根据它发奖励或扣血。
enum class RoundResult {
    None,
    PlayerVictory,
    PlayerDefeat
};

// 整局游戏结果，GameOver 时由它区分胜利和失败。
enum class MatchResult {
    Ongoing,
    PlayerVictory,
    PlayerDefeat
};

// Unit 的当前位置类型。具体槽位或格子保存在 Placement 里。
enum class PlacementKind {
    None,
    BenchSlot,
    BoardCell
};

// 放置到已有单位位置时的处理策略：拒绝或交换。
enum class PlacementPolicy {
    Reject,
    Swap
};

// 棋盘分成上下半场，敌人在上半，玩家在下半。
enum class BoardHalf {
    Enemy,
    Player
};

// 装备目前支持的数值修改类型。
enum class ItemEffectType {
    Attack,
    MaxHp,
    MaxMana,
    SkillManaCost,
    AttackIntervalPercent
};

struct Position {
    int row = 0;
    int col = 0;

    // Position 是小值类型，直接按 row/col 比较即可。
    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }

    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
};

// 命令式接口统一返回成功标记和一段给 GUI/CLI 展示的信息。
struct ActionResult {
    bool success = false;
    std::string message;
};

// 单位的基础战斗属性。间隔越小，攻击或移动越快。
struct UnitStats {
    // 这些数值是“规则层单位”的完整战斗参数，GUI 不应自己推导或缓存另一套副本。
    // 装备、星级和羁绊会从 baseStats 计算出 effectiveStats，再由 Unit 的访问器统一读取。
    int maxHp = 1;
    int atk = 1;
    int range = 1;
    int maxMana = 60;
    int attackInterval = 60;
    int moveInterval = 20;
    int initialMana = 0;
    int manaRegenPerSecond = 0;
    int skillManaCost = 60;
    int skillCooldownTicks = 0;
};

// 可购买单位的静态定义。definitionId/factoryKey/visualKey 是稳定逻辑键。
struct UnitDefinition {
    // definitionId 用于商店、存档和三合一判断，必须比显示名更稳定。
    std::string definitionId;
    std::string name;
    int cost = 1;
    std::vector<std::string> traits;
    UnitStats baseStats;
    // factoryKey 决定实例化哪一种 Unit 派生类，visualKey 决定 GUI 贴图资源。
    std::string factoryKey;
    std::string visualKey;
    std::string star1VisualKey;
    std::string star2VisualKey;
};

// 商店槽位只保存定义 ID 和价格，购买时再从目录创建 Unit。
struct ShopOffer {
    std::string definitionId;
    int cost = 0;
};

// 装备定义描述装备如何修改 UnitStats。
struct ItemDefinition {
    std::string itemDefId;
    std::string name;
    std::string visualKey;
    ItemEffectType effectType = ItemEffectType::Attack;
    int value = 0;
};

// 羁绊定义主要服务展示，真正的阈值和数值效果在 GameState 中计算。
struct TraitDefinition {
    std::string traitId;
    std::string displayName;
    std::string visualKey;
    std::string description;
};

// 装备实例有独立 itemId，同一种装备可以掉落多件。
struct ItemInstance {
    ItemId itemId = 0;
    std::string itemDefId;
};

// 当前羁绊状态，供属性计算和 GUI 面板共用。
struct SynergyStatus {
    std::string trait;
    int count = 0;
    int activeThreshold = 0;
    int nextThreshold = 0;
    bool active = false;
    std::string effectDescription;
};

// 战斗和结算参数集中放在这里，便于测试覆盖不同节奏。
struct CombatConfig {
    int attackInterval = 60;
    int moveInterval = 20;
    int victoryGold = 5;
    int defeatGold = 2;
    int defeatHpLoss = 10;
    int maxRound = 5;
    // 验收演示时希望每次胜利都能讲到装备系统，所以默认胜利掉落率设为 100%。
    // 需要测试“无掉落”路径时，可以像 stage3 测试那样单独把它改成 0。
    int itemDropPercent = 100;
};

// Unit 的单一位置来源。修改位置时必须同步 Board/Bench 的占用表。
struct Placement {
    PlacementKind kind = PlacementKind::None;
    std::optional<std::size_t> benchSlot;
    std::optional<Position> boardCell;

    // 以下工厂函数保证 kind 与附带坐标/槽位一起设置，减少手写 Placement 时的半初始化状态。
    static Placement none() {
        return {};
    }

    static Placement bench(std::size_t slot) {
        // BenchSlot 只使用 benchSlot 字段；boardCell 保持空值，用来表示互斥的当前位置来源。
        Placement placement;
        placement.kind = PlacementKind::BenchSlot;
        placement.benchSlot = slot;
        return placement;
    }

    static Placement board(Position position) {
        // BoardCell 只使用 boardCell 字段。Board 的占用表和 Unit::placement 必须由 GameState 同步维护。
        Placement placement;
        placement.kind = PlacementKind::BoardCell;
        placement.boardCell = position;
        return placement;
    }
};

// toString 系列函数主要服务三类场景：控制台演示、存档文本格式和调试面板。
// 因此返回值应保持稳定、简短，并尽量避免依赖本地化显示文本。
std::string toString(Owner owner);
std::string toString(UnitState state);
std::string toString(GamePhase phase);
std::string toString(RoundResult result);
std::string toString(MatchResult result);
std::string toString(ItemEffectType effectType);
std::string toString(const Position& position);
std::string toString(const Placement& placement);
std::string toString(const UnitStats& stats);

}  // namespace synera
