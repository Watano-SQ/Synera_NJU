#pragma once

#include "Bench.h"
#include "Board.h"
#include "Player.h"
#include "Types.h"
#include "UnitManager.h"

#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace synera {

class EncounterGenerator;
struct CombatEffect;

// GameState 是核心规则总入口：持有玩家、棋盘、备战区、单位、商店、装备、羁绊和战斗状态。
// GUI、CLI 和测试都应通过这里改状态，而不是直接写 Board/Bench/UnitManager。
class GameState {
public:
    GameState(int rows = 8,
              int cols = 8,
              std::size_t benchCapacity = 8,
              CombatConfig combatConfig = CombatConfig{});
    GameState(GameState&&) noexcept = default;
    GameState& operator=(GameState&&) noexcept = default;
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    Player& player();
    const Player& player() const;
    Board& board();
    const Board& board() const;
    Bench& bench();
    const Bench& bench() const;
    UnitManager& units();
    const UnitManager& units() const;
    GamePhase phase() const;
    RoundResult lastRoundResult() const;
    MatchResult matchResult() const;
    const CombatConfig& combatConfig() const;
    const std::vector<std::optional<ShopOffer>>& shopOffers() const;
    std::vector<ItemInstance> equipmentInventory() const;
    std::optional<ItemInstance> item(ItemId itemId) const;
    const std::vector<SynergyStatus>& activeSynergies() const;
    int deployedPlayerUnitCount() const;

    Unit* unit(UnitId id);
    const Unit* unit(UnitId id) const;
    void setCombatConfig(CombatConfig config);
    void setItemDropPercent(int percent);

    // 准备阶段的经济和阵容命令。
    UnitId addUnitToBench(std::unique_ptr<Unit> unit);
    ItemId addItemToInventory(const std::string& itemDefId);
    ActionResult refreshShop();
    ActionResult purchaseShopSlot(std::size_t index);
    ActionResult upgradePopulation();
    ActionResult equipItem(ItemId itemId, UnitId unitId);
    UnitStats computeEffectiveStats(UnitId id);
    // 存档只支持非 Combat 阶段，读档会先校验到临时状态再替换当前状态。
    ActionResult saveToFile(const std::string& path) const;
    ActionResult loadFromFile(const std::string& path);
    // 放置接口用于 Bench <-> Board 和 Board 内移动，所有半场/人口/交换规则都在实现里检查。
    bool deployFromBench(std::size_t slot, Position target, PlacementPolicy policy = PlacementPolicy::Reject);
    bool moveBoardUnit(Position from, Position to, PlacementPolicy policy = PlacementPolicy::Reject);
    bool returnToBench(Position from, std::size_t slot);
    // 战斗状态机：Prep -> Combat -> Resolve -> Prep/GameOver。
    std::vector<UnitId> generateEnemiesForRound(int round);
    ActionResult startCombat();
    ActionResult tickCombat();
    ActionResult resolveRound();

    std::vector<UnitId> activePlayerUnits() const;
    std::vector<UnitId> activeEnemyUnits() const;
    std::vector<UnitId> activeCombatUnits() const;
    const std::vector<UnitId>& currentEnemyUnits() const;

private:
    friend class EncounterGenerator;
    friend class SkillContext;

    // 每个战斗单位的临时冷却和当前目标，不写入存档。
    struct RuntimeState {
        int attackCooldown = 0;
        int moveCooldown = 0;
        int skillCooldown = 0;
        int manaRegenRemainder = 0;
        std::optional<UnitId> currentTarget;
    };

    // 一个 tick 中先收集移动提案，统一检测目标格冲突后再执行。
    struct MoveProposal {
        UnitId unitId = 0;
        Position from;
        Position to;
    };

    bool canOccupyHalf(const Unit& unit, Position position) const;
    UnitId addEnemyToBoard(std::unique_ptr<Unit> unit, Position position);
    void clearGeneratedEnemies();
    void clearAllEnemyUnits();
    void clearBoardOccupant(Position position);
    void clearBoardForSettlement();
    // 战斗循环内部步骤。
    void initializeRuntime();
    void updateCooldowns(const std::vector<UnitId>& activeUnits);
    std::optional<UnitId> selectTarget(UnitId attackerId) const;
    std::optional<Position> nextStepTowardAttackRange(UnitId attackerId, UnitId targetId) const;
    bool isInAttackRange(const Unit& attacker, const Unit& target) const;
    void applyCombatEffects(const std::vector<CombatEffect>& damageEvents,
                            const std::vector<CombatEffect>& healEvents);
    void applyMoveProposals(const std::vector<MoveProposal>& proposals);
    void clearDeadBoardOccupants();
    void checkCombatEnd();
    void restorePlayerSnapshot(std::string& message);
    std::vector<UnitId> activeUnitsForOwner(Owner owner) const;
    // 准备阶段的商店、羁绊、属性和自动合成逻辑。
    void generateShopOffersFree();
    void recomputePrepSynergiesAndStats();
    std::vector<SynergyStatus> computeSynergiesFromBoard() const;
    void recomputeAllPlayerEffectiveStats();
    void maybeMergeUnit(UnitId newestId);
    void removeUnitFromPlacement(UnitId id);
    void returnEquippedItemToInventory(Unit& unit);
    bool hasItemInInventory(ItemId itemId) const;
    const ItemInstance* itemInstance(ItemId itemId) const;
    ItemInstance* itemInstance(ItemId itemId);
    void dropVictoryItemIfNeeded();
    int settlementGoldBonusFromSynergies() const;
    bool ownerHasActiveSynergy(Owner owner, const std::string& trait) const;
    // 读档和阶段切换时用来重建或清理派生状态。
    void rebuildOccupancyFromUnitPlacements();
    void clearRuntimeOnlyState();

    // 持久核心状态。
    Player player_;
    Board board_;
    Bench bench_;
    UnitManager units_;
    GamePhase phase_ = GamePhase::Prep;
    RoundResult lastRoundResult_ = RoundResult::None;
    MatchResult matchResult_ = MatchResult::Ongoing;
    CombatConfig combatConfig_;
    std::vector<std::optional<ShopOffer>> shopOffers_;
    std::unordered_map<ItemId, ItemInstance> itemInstances_;
    std::vector<ItemId> equipmentInventory_;
    std::vector<SynergyStatus> activeSynergies_;
    ItemId nextItemId_ = 1;
    std::uint64_t nextAcquireSeq_ = 1;
    std::mt19937 rng_;
    // 以下是战斗运行时/结算辅助状态。
    std::vector<UnitId> currentEnemyUnits_;
    std::vector<std::pair<UnitId, Position>> playerCombatSnapshot_;
    std::unordered_map<UnitId, RuntimeState> runtime_;
};

}  // namespace synera
