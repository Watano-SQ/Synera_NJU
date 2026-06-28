#include "GameState.h"

#include "Catalog.h"
#include "EncounterGenerator.h"
#include "SkillContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace synera {

namespace {

int distanceSquared(Position a, Position b) {
    // 战斗范围只需要比较大小，用距离平方可以避免开方。
    const int dr = a.row - b.row;
    const int dc = a.col - b.col;
    return dr * dr + dc * dc;
}

int positionKey(const Board& board, Position position) {
    // 把二维位置压成唯一整数，方便放进 set/map 做冲突检测。
    return position.row * board.cols() + position.col;
}

Owner opposingOwner(Owner owner) {
    return owner == Owner::PlayerCtrl ? Owner::EnemyCtrl : Owner::PlayerCtrl;
}

std::optional<Position> boardPositionOf(const Unit* unit) {
    // 只有棋盘上的单位才参与寻路、攻击距离和范围技能。
    if (unit == nullptr || unit->placement().kind != PlacementKind::BoardCell) {
        return std::nullopt;
    }
    return unit->placement().boardCell;
}

bool hasTrait(const Unit& unit, const std::string& trait) {
    // 羁绊判断基于稳定 traitId，不依赖可能乱码的显示名。
    const auto& traits = unit.traits();
    return std::find(traits.begin(), traits.end(), trait) != traits.end();
}

int percentAdjusted(int value, int percentDelta) {
    // 攻击间隔类加成使用百分比，并保证结果至少为 1 tick。
    return std::max(1, value + (value * percentDelta) / 100);
}

int adjustedSkillManaCost(int value, int delta) {
    if (value <= 0) {
        return 0;
    }
    return std::max(10, value + delta);
}

int targetBaseThreat(const Unit& target) {
    // 基础威胁由“能打多疼、能打多远、出手多快、身体有多厚”组成，保持整数权重便于验收讲解。
    return target.atk() * 10 + target.range() * 8 + std::max(0, 80 - target.attackInterval()) +
           target.maxHp() / 25;
}

int targetSkillThreat(const Unit& target, int runtimeSkillCooldown) {
    if (!target.hasActiveSkill() || target.skillManaCost() <= 0) {
        return 0;
    }

    int score = 15;
    if (target.mana() >= target.skillManaCost() && runtimeSkillCooldown == 0) {
        return score + 45;
    }
    score += std::min(30, target.mana() * 30 / target.skillManaCost());
    return score;
}

int executeValue(const Unit& attacker, const Unit& target) {
    if (target.hp() <= attacker.atk()) {
        return 35;
    }
    if (target.hp() <= attacker.atk() * 2) {
        return 15;
    }
    return 0;
}

std::string placementKindToken(PlacementKind kind) {
    switch (kind) {
        case PlacementKind::None:
            return "NONE";
        case PlacementKind::BenchSlot:
            return "BENCH";
        case PlacementKind::BoardCell:
            return "BOARD";
    }
    return "NONE";
}

std::optional<GamePhase> parsePhase(const std::string& token) {
    if (token == "Prep") {
        return GamePhase::Prep;
    }
    if (token == "Combat") {
        return GamePhase::Combat;
    }
    if (token == "Resolve") {
        return GamePhase::Resolve;
    }
    if (token == "GameOver") {
        return GamePhase::GameOver;
    }
    return std::nullopt;
}

std::optional<Owner> parseOwner(const std::string& token) {
    if (token == "PlayerCtrl") {
        return Owner::PlayerCtrl;
    }
    if (token == "EnemyCtrl") {
        return Owner::EnemyCtrl;
    }
    return std::nullopt;
}

std::optional<RoundResult> parseRoundResult(const std::string& token) {
    if (token == "None") {
        return RoundResult::None;
    }
    if (token == "PlayerVictory") {
        return RoundResult::PlayerVictory;
    }
    if (token == "PlayerDefeat") {
        return RoundResult::PlayerDefeat;
    }
    return std::nullopt;
}

std::optional<MatchResult> parseMatchResult(const std::string& token) {
    if (token == "Ongoing") {
        return MatchResult::Ongoing;
    }
    if (token == "PlayerVictory") {
        return MatchResult::PlayerVictory;
    }
    if (token == "PlayerDefeat") {
        return MatchResult::PlayerDefeat;
    }
    return std::nullopt;
}

}  // namespace

GameState::GameState(int rows, int cols, std::size_t benchCapacity, CombatConfig combatConfig)
    : board_(rows, cols),
      bench_(benchCapacity),
      combatConfig_(combatConfig),
      shopOffers_(5),
      rng_(std::random_device{}()) {
    // 新游戏默认先生成一组免费商店，之后刷新才会扣金币。
    generateShopOffersFree();
}

Player& GameState::player() {
    return player_;
}

const Player& GameState::player() const {
    return player_;
}

Board& GameState::board() {
    return board_;
}

const Board& GameState::board() const {
    return board_;
}

Bench& GameState::bench() {
    return bench_;
}

const Bench& GameState::bench() const {
    return bench_;
}

UnitManager& GameState::units() {
    return units_;
}

const UnitManager& GameState::units() const {
    return units_;
}

GamePhase GameState::phase() const {
    return phase_;
}

RoundResult GameState::lastRoundResult() const {
    return lastRoundResult_;
}

MatchResult GameState::matchResult() const {
    return matchResult_;
}

const CombatConfig& GameState::combatConfig() const {
    return combatConfig_;
}

const std::vector<std::optional<ShopOffer>>& GameState::shopOffers() const {
    return shopOffers_;
}

std::vector<ItemInstance> GameState::equipmentInventory() const {
    std::vector<ItemInstance> result;
    result.reserve(equipmentInventory_.size());
    // inventory_ 只保存 itemId，返回给外部时展开成完整实例并排序。
    for (ItemId itemId : equipmentInventory_) {
        const auto it = itemInstances_.find(itemId);
        if (it != itemInstances_.end()) {
            result.push_back(it->second);
        }
    }
    std::sort(result.begin(), result.end(), [](const ItemInstance& lhs, const ItemInstance& rhs) {
        return lhs.itemId < rhs.itemId;
    });
    return result;
}

std::optional<ItemInstance> GameState::item(ItemId itemId) const {
    const auto it = itemInstances_.find(itemId);
    if (it == itemInstances_.end()) {
        return std::nullopt;
    }
    return it->second;
}

const std::vector<SynergyStatus>& GameState::activeSynergies() const {
    return activeSynergies_;
}

int GameState::deployedPlayerUnitCount() const {
    int count = 0;
    // 人口只统计已经上场的玩家单位，Bench 中的单位不占人口。
    for (int row = 0; row < board_.rows(); ++row) {
        for (int col = 0; col < board_.cols(); ++col) {
            const auto occupant = board_.occupant(Position{row, col});
            if (!occupant.has_value()) {
                continue;
            }
            const Unit* unitOnCell = unit(*occupant);
            if (unitOnCell != nullptr && unitOnCell->owner() == Owner::PlayerCtrl) {
                ++count;
            }
        }
    }
    return count;
}

Unit* GameState::unit(UnitId id) {
    return units_.get(id);
}

const Unit* GameState::unit(UnitId id) const {
    return units_.get(id);
}

void GameState::setCombatConfig(CombatConfig config) {
    combatConfig_ = config;
}

void GameState::setItemDropPercent(int percent) {
    combatConfig_.itemDropPercent = std::clamp(percent, 0, 100);
}

ItemId GameState::addItemToInventory(const std::string& itemDefId) {
    if (findItemDefinition(itemDefId) == nullptr) {
        throw std::invalid_argument("Unknown item definition id.");
    }
    // itemDefId 是装备类型，itemId 是这件装备实例的唯一编号。
    const ItemId itemId = nextItemId_++;
    itemInstances_.emplace(itemId, ItemInstance{itemId, itemDefId});
    equipmentInventory_.push_back(itemId);
    return itemId;
}

ActionResult GameState::refreshShop() {
    // 商店操作只允许在 Prep，战斗中不能改变阵容来源。
    if (phase_ != GamePhase::Prep) {
        return {false, "只能在准备阶段刷新商店。"};
    }
    constexpr int refreshCost = 2;
    if (player_.gold() < refreshCost) {
        return {false, "阳光不足，无法刷新商店。"};
    }
    player_.setGold(player_.gold() - refreshCost);
    generateShopOffersFree();
    return {true, "商店已刷新。"};
}

ActionResult GameState::purchaseShopSlot(std::size_t index) {
    // 购买流程：校验阶段/槽位/金币/Bench 空位，创建单位，再尝试三合一。
    if (phase_ != GamePhase::Prep) {
        return {false, "只能在准备阶段种下植物。"};
    }
    if (index >= shopOffers_.size()) {
        return {false, "商店位置无效。"};
    }
    if (!shopOffers_[index].has_value()) {
        return {false, "这个商店位置是空的。"};
    }

    const ShopOffer offer = *shopOffers_[index];
    const UnitDefinition* definition = findUnitDefinition(offer.definitionId);
    if (definition == nullptr) {
        return {false, "商店植物定义不存在。"};
    }
    if (player_.gold() < offer.cost) {
        return {false, "阳光不足，无法种下植物。"};
    }
    if (!bench_.firstEmptySlot().has_value()) {
        return {false, "备战区已满。"};
    }

    player_.setGold(player_.gold() - offer.cost);
    std::unique_ptr<Unit> unit = createUnitFromDefinition(*definition, Owner::PlayerCtrl);
    const UnitId id = addUnitToBench(std::move(unit));
    maybeMergeUnit(id);
    shopOffers_[index].reset();
    recomputePrepSynergiesAndStats();
    return {true, "植物已加入备战区。"};
}

ActionResult GameState::upgradePopulation() {
    // 人口升级同时提高 level 和上场上限，花费随当前等级递增。
    if (phase_ != GamePhase::Prep) {
        return {false, "只能在准备阶段升级人口。"};
    }
    constexpr int maxLevel = 6;
    constexpr int maxUnitCap = 8;
    if (player_.level() >= maxLevel) {
        return {false, "人口等级已达到上限。"};
    }
    const int cost = 4 + 2 * (player_.level() - 1);
    if (player_.gold() < cost) {
        return {false, "阳光不足，无法升级人口。"};
    }

    player_.setGold(player_.gold() - cost);
    player_.setLevel(player_.level() + 1);
    player_.setUnitCap(std::min(maxUnitCap, player_.unitCap() + 1));
    return {true, "人口已升级。"};
}

ActionResult GameState::equipItem(ItemId itemId, UnitId unitId) {
    // 装备从库存移动到单位身上。当前规则限制每个单位最多装备一件。
    if (phase_ != GamePhase::Prep) {
        return {false, "只能在准备阶段穿戴装备。"};
    }
    if (!hasItemInInventory(itemId)) {
        return {false, "装备不在库存中。"};
    }
    Unit* target = unit(unitId);
    if (target == nullptr || target->owner() != Owner::PlayerCtrl) {
        return {false, "只有玩家植物可以穿戴装备。"};
    }
    if (target->equippedItemId().has_value()) {
        return {false, "该植物已经有装备。"};
    }

    equipmentInventory_.erase(std::remove(equipmentInventory_.begin(), equipmentInventory_.end(), itemId),
                              equipmentInventory_.end());
    target->setEquippedItemId(itemId);
    computeEffectiveStats(unitId);
    recomputePrepSynergiesAndStats();
    return {true, "装备已穿戴。"};
}

UnitStats GameState::computeEffectiveStats(UnitId id) {
    Unit* target = unit(id);
    if (target == nullptr) {
        return {};
    }

    UnitStats stats = target->baseStats();
    // 最终属性 = 基础属性 + 星级倍率 + 装备 + 当前激活羁绊。
    if (target->star() == 2) {
        stats.maxHp = static_cast<int>(std::lround(stats.maxHp * 1.7));
        stats.atk = static_cast<int>(std::lround(stats.atk * 1.7));
    }

    if (target->equippedItemId().has_value()) {
        const ItemInstance* instance = itemInstance(*target->equippedItemId());
        const ItemDefinition* definition = instance != nullptr ? findItemDefinition(instance->itemDefId) : nullptr;
        if (definition != nullptr) {
            switch (definition->effectType) {
                case ItemEffectType::Attack:
                    stats.atk += definition->value;
                    break;
                case ItemEffectType::MaxHp:
                    stats.maxHp += definition->value;
                    break;
                case ItemEffectType::MaxMana:
                    stats.maxMana = std::max(10, stats.maxMana + definition->value);
                    break;
                case ItemEffectType::SkillManaCost:
                    stats.skillManaCost = adjustedSkillManaCost(stats.skillManaCost, definition->value);
                    break;
                case ItemEffectType::AttackIntervalPercent:
                    stats.attackInterval = percentAdjusted(stats.attackInterval, definition->value);
                    break;
            }
        }
    }

    for (const SynergyStatus& synergy : activeSynergies_) {
        if (target->owner() != Owner::PlayerCtrl || !synergy.active || !hasTrait(*target, synergy.trait)) {
            continue;
        }
        if (synergy.trait == "shooter") {
            stats.attackInterval = percentAdjusted(stats.attackInterval, synergy.activeThreshold >= 4 ? -20 : -10);
        } else if (synergy.trait == "nut") {
            stats.maxHp += synergy.activeThreshold >= 4 ? 450 : 200;
        } else if (synergy.trait == "fungus") {
            stats.skillManaCost = adjustedSkillManaCost(stats.skillManaCost, synergy.activeThreshold >= 4 ? -25 : -10);
        } else if (synergy.trait == "spike") {
            stats.atk += 15;
        }
    }

    target->setEffectiveStats(stats);
    return target->effectiveStats();
}

ActionResult GameState::saveToFile(const std::string& path) const {
    // 战斗中存在临时敌人、冷却和移动提案，当前存档格式只保存非 Combat 状态。
    if (phase_ == GamePhase::Combat) {
        return {false, "守家阶段不能存档。"};
    }

    std::ofstream out(path);
    if (!out) {
        return {false, "无法打开存档文件进行写入。"};
    }

    out << "SYNERA_SAVE 1\n";
    // 存档使用稳定的文本格式，便于测试和手工排查。
    out << "BOARD " << board_.rows() << ' ' << board_.cols() << "\n";
    out << "BENCH " << bench_.capacity() << "\n";
    out << "PLAYER " << player_.hp() << ' ' << player_.gold() << ' ' << player_.level() << ' '
        << player_.unitCap() << ' ' << player_.currentRound() << "\n";
    out << "STATE " << toString(phase_) << ' ' << toString(lastRoundResult_) << ' ' << toString(matchResult_)
        << ' ' << units_.nextId() << ' ' << nextItemId_ << ' ' << nextAcquireSeq_ << "\n";

    out << "SHOP " << shopOffers_.size() << "\n";
    for (std::size_t i = 0; i < shopOffers_.size(); ++i) {
        if (shopOffers_[i].has_value()) {
            out << "OFFER " << i << ' ' << shopOffers_[i]->definitionId << ' ' << shopOffers_[i]->cost << "\n";
        } else {
            out << "OFFER " << i << " EMPTY 0\n";
        }
    }

    std::vector<ItemId> itemIds;
    itemIds.reserve(itemInstances_.size());
    for (const auto& [itemId, instance] : itemInstances_) {
        itemIds.push_back(itemId);
    }
    std::sort(itemIds.begin(), itemIds.end());
    out << "ITEMS " << itemIds.size() << "\n";
    for (ItemId itemId : itemIds) {
        const ItemInstance& instance = itemInstances_.at(itemId);
        UnitId equippedBy = 0;
        // 装备实例本身存在 itemInstances_，位置可能是库存或某个单位身上。
        for (UnitId unitId : units_.ids()) {
            const Unit* candidate = unit(unitId);
            if (candidate != nullptr && candidate->equippedItemId() == itemId) {
                equippedBy = unitId;
                break;
            }
        }
        if (hasItemInInventory(itemId)) {
            out << "ITEM " << itemId << ' ' << instance.itemDefId << " INVENTORY 0\n";
        } else {
            out << "ITEM " << itemId << ' ' << instance.itemDefId << " EQUIPPED " << equippedBy << "\n";
        }
    }

    std::vector<UnitId> unitIds;
    for (UnitId id : units_.ids()) {
        const Unit* savedUnit = unit(id);
        if (savedUnit != nullptr && savedUnit->owner() == Owner::PlayerCtrl) {
            unitIds.push_back(id);
        }
    }
    out << "UNITS " << unitIds.size() << "\n";
    for (UnitId id : unitIds) {
        const Unit* savedUnit = unit(id);
        if (savedUnit == nullptr) {
            continue;
        }
        const Placement& placement = savedUnit->placement();
        int slot = -1;
        int row = -1;
        int col = -1;
        if (placement.kind == PlacementKind::BenchSlot && placement.benchSlot.has_value()) {
            slot = static_cast<int>(*placement.benchSlot);
        } else if (placement.kind == PlacementKind::BoardCell && placement.boardCell.has_value()) {
            row = placement.boardCell->row;
            col = placement.boardCell->col;
        }
        out << "UNIT " << id << ' ' << savedUnit->definitionId() << ' ' << toString(savedUnit->owner()) << ' '
            << savedUnit->star() << ' ' << savedUnit->acquireSeq() << ' ' << savedUnit->hp() << ' '
            << savedUnit->mana() << ' ' << savedUnit->equippedItemId().value_or(0) << ' '
            << placementKindToken(placement.kind) << ' ' << slot << ' ' << row << ' ' << col << "\n";
    }

    out << "SNAPSHOTS " << playerCombatSnapshot_.size() << "\n";
    for (const auto& [id, position] : playerCombatSnapshot_) {
        out << "SNAPSHOT " << id << ' ' << position.row << ' ' << position.col << "\n";
    }
    out << "END\n";
    return {true, "Game saved."};
}

ActionResult GameState::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return {false, "无法打开存档文件进行读取。"};
    }

    auto fail = [](const std::string& message) { return ActionResult{false, message}; };
    // 先读入临时 GameState，全部校验通过后再 move 到 *this，避免坏存档污染当前游戏。
    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "SYNERA_SAVE" || version != 1) {
        return fail("Invalid or unsupported save version.");
    }

    int rows = 0;
    int cols = 0;
    if (!(in >> tag >> rows >> cols) || tag != "BOARD" || rows <= 0 || cols <= 0) {
        return fail("Invalid board section.");
    }
    std::size_t benchCapacity = 0;
    if (!(in >> tag >> benchCapacity) || tag != "BENCH" || benchCapacity == 0) {
        return fail("Invalid bench section.");
    }

    GameState temp(rows, cols, benchCapacity, combatConfig_);
    // 构造函数会生成默认商店，这里清空后按存档逐段恢复。
    temp.shopOffers_.assign(5, std::nullopt);
    temp.itemInstances_.clear();
    temp.equipmentInventory_.clear();
    temp.units_.clear();
    temp.currentEnemyUnits_.clear();
    temp.playerCombatSnapshot_.clear();
    temp.runtime_.clear();

    int playerHp = 0;
    int playerGold = 0;
    int playerLevel = 0;
    int playerCap = 0;
    int playerRound = 0;
    if (!(in >> tag >> playerHp >> playerGold >> playerLevel >> playerCap >> playerRound) || tag != "PLAYER") {
        return fail("Invalid player section.");
    }
    temp.player_.setHp(playerHp);
    temp.player_.setGold(playerGold);
    temp.player_.setLevel(playerLevel);
    temp.player_.setUnitCap(playerCap);
    temp.player_.setCurrentRound(playerRound);

    std::string phaseToken;
    std::string roundToken;
    std::string matchToken;
    UnitId nextUnitId = 1;
    if (!(in >> tag >> phaseToken >> roundToken >> matchToken >> nextUnitId >> temp.nextItemId_ >>
          temp.nextAcquireSeq_) ||
        tag != "STATE") {
        return fail("Invalid state section.");
    }
    const auto parsedPhase = parsePhase(phaseToken);
    const auto parsedRound = parseRoundResult(roundToken);
    const auto parsedMatch = parseMatchResult(matchToken);
    if (!parsedPhase.has_value() || !parsedRound.has_value() || !parsedMatch.has_value() ||
        *parsedPhase == GamePhase::Combat) {
        return fail("Invalid saved phase.");
    }
    temp.phase_ = *parsedPhase;
    temp.lastRoundResult_ = *parsedRound;
    temp.matchResult_ = *parsedMatch;
    temp.units_.setNextId(nextUnitId);

    std::size_t shopCount = 0;
    if (!(in >> tag >> shopCount) || tag != "SHOP" || shopCount != 5) {
        return fail("Invalid shop section.");
    }
    for (std::size_t i = 0; i < shopCount; ++i) {
        std::size_t index = 0;
        std::string definitionId;
        int cost = 0;
        if (!(in >> tag >> index >> definitionId >> cost) || tag != "OFFER" || index >= shopCount) {
            return fail("Invalid shop offer.");
        }
        if (definitionId != "EMPTY") {
            const UnitDefinition* definition = findUnitDefinition(definitionId);
            if (definition == nullptr) {
                return fail("Shop offer has unknown definitionId.");
            }
            temp.shopOffers_[index] = ShopOffer{definitionId, cost};
        }
    }

    std::unordered_map<ItemId, std::pair<std::string, UnitId>> itemLocations;
    // 先记录装备实例及位置，等单位全部创建后再校验 EQUIPPED 归属。
    std::size_t itemCount = 0;
    if (!(in >> tag >> itemCount) || tag != "ITEMS") {
        return fail("Invalid items section.");
    }
    for (std::size_t i = 0; i < itemCount; ++i) {
        ItemId itemId = 0;
        std::string itemDefId;
        std::string location;
        UnitId locationUnit = 0;
        if (!(in >> tag >> itemId >> itemDefId >> location >> locationUnit) || tag != "ITEM" || itemId == 0) {
            return fail("Invalid item entry.");
        }
        if (findItemDefinition(itemDefId) == nullptr) {
            return fail("Item entry has unknown itemDefId.");
        }
        if (!temp.itemInstances_.emplace(itemId, ItemInstance{itemId, itemDefId}).second ||
            !itemLocations.emplace(itemId, std::make_pair(location, locationUnit)).second) {
            return fail("Duplicate itemId in save.");
        }
    }

    struct PendingVitals {
        UnitId id = 0;
        int hp = 0;
        int mana = 0;
    };
    std::vector<PendingVitals> pendingVitals;
    // 读单位时先恢复定义、星级、装备和位置，生命/法力要等最终属性计算后再写回。
    std::unordered_set<UnitId> seenUnitIds;
    std::set<std::size_t> occupiedBenchSlots;
    std::set<int> occupiedBoardCells;
    std::size_t unitCount = 0;
    if (!(in >> tag >> unitCount) || tag != "UNITS") {
        return fail("Invalid units section.");
    }
    for (std::size_t i = 0; i < unitCount; ++i) {
        UnitId unitId = 0;
        std::string definitionId;
        std::string ownerToken;
        int star = 0;
        std::uint64_t acquireSeq = 0;
        int hp = 0;
        int mana = 0;
        ItemId equippedItemId = 0;
        std::string placementToken;
        int slot = -1;
        int row = -1;
        int col = -1;
        if (!(in >> tag >> unitId >> definitionId >> ownerToken >> star >> acquireSeq >> hp >> mana >> equippedItemId >>
              placementToken >> slot >> row >> col) ||
            tag != "UNIT") {
            return fail("Invalid unit entry.");
        }
        if (unitId == 0 || !seenUnitIds.insert(unitId).second) {
            return fail("Duplicate or zero UnitId in save.");
        }
        const UnitDefinition* definition = findUnitDefinition(definitionId);
        const auto owner = parseOwner(ownerToken);
        if (definition == nullptr || !owner.has_value() || (star != 1 && star != 2)) {
            return fail("Unit entry has invalid definition, owner, or star.");
        }

        std::unique_ptr<Unit> loadedUnit = createUnitFromDefinition(*definition, *owner);
        loadedUnit->setStar(star);
        loadedUnit->setAcquireSeq(acquireSeq);
        if (equippedItemId != 0) {
            if (temp.itemInstances_.find(equippedItemId) == temp.itemInstances_.end()) {
                return fail("Unit references unknown equippedItemId.");
            }
            loadedUnit->setEquippedItemId(equippedItemId);
        }

        if (placementToken == "NONE") {
            loadedUnit->setPlacement(Placement::none());
        } else if (placementToken == "BENCH") {
            if (*owner != Owner::PlayerCtrl || slot < 0 || !temp.bench_.isValidSlot(static_cast<std::size_t>(slot)) ||
                !occupiedBenchSlots.insert(static_cast<std::size_t>(slot)).second) {
                return fail("Invalid or conflicting bench placement.");
            }
            loadedUnit->setPlacement(Placement::bench(static_cast<std::size_t>(slot)));
        } else if (placementToken == "BOARD") {
            const Position position{row, col};
            if (!temp.board_.isInside(position) || !temp.canOccupyHalf(*loadedUnit, position) ||
                !occupiedBoardCells.insert(positionKey(temp.board_, position)).second) {
                return fail("Invalid or conflicting board placement.");
            }
            loadedUnit->setPlacement(Placement::board(position));
        } else {
            return fail("Unknown placement token.");
        }

        temp.units_.addWithId(unitId, std::move(loadedUnit));
        pendingVitals.push_back(PendingVitals{unitId, hp, mana});
    }

    for (const auto& [itemId, location] : itemLocations) {
        if (location.first == "INVENTORY") {
            temp.equipmentInventory_.push_back(itemId);
        } else if (location.first == "EQUIPPED") {
            const Unit* equippedUnit = temp.unit(location.second);
            if (equippedUnit == nullptr || equippedUnit->equippedItemId() != itemId) {
                return fail("Equipped item ownership is invalid.");
            }
        } else {
            return fail("Unknown item location.");
        }
    }
    for (UnitId id : temp.units_.ids()) {
        const Unit* loadedUnit = temp.unit(id);
        if (loadedUnit != nullptr && loadedUnit->equippedItemId().has_value()) {
            const auto it = itemLocations.find(*loadedUnit->equippedItemId());
            if (it == itemLocations.end() || it->second.first != "EQUIPPED" || it->second.second != id) {
                return fail("Unit equipped item does not match item ownership.");
            }
        }
    }

    std::size_t snapshotCount = 0;
    if (!(in >> tag >> snapshotCount) || tag != "SNAPSHOTS") {
        return fail("Invalid snapshot section.");
    }
    for (std::size_t i = 0; i < snapshotCount; ++i) {
        UnitId id = 0;
        Position position;
        if (!(in >> tag >> id >> position.row >> position.col) || tag != "SNAPSHOT" || temp.unit(id) == nullptr ||
            !temp.board_.isPlayerHalf(position)) {
            return fail("Invalid snapshot entry.");
        }
        temp.playerCombatSnapshot_.push_back({id, position});
    }
    if (!(in >> tag) || tag != "END") {
        return fail("Save file did not terminate correctly.");
    }

    temp.rebuildOccupancyFromUnitPlacements();
    temp.activeSynergies_ = temp.computeSynergiesFromBoard();
    temp.recomputeAllPlayerEffectiveStats();
    // 属性恢复完成后再钳制 HP/Mana，防止装备和羁绊改变上限后数值不合法。
    for (const PendingVitals& vitals : pendingVitals) {
        Unit* loadedUnit = temp.unit(vitals.id);
        if (loadedUnit != nullptr) {
            loadedUnit->setHp(vitals.hp);
            loadedUnit->setMana(vitals.mana);
        }
    }
    temp.clearRuntimeOnlyState();
    temp.currentEnemyUnits_.clear();
    temp.units_.setNextId(nextUnitId);
    *this = std::move(temp);
    return {true, "Game loaded."};
}

UnitId GameState::addUnitToBench(std::unique_ptr<Unit> unit) {
    if (!unit || unit->owner() != Owner::PlayerCtrl) {
        throw std::invalid_argument("Only PlayerCtrl units can be added to the player bench.");
    }

    const auto slot = bench_.firstEmptySlot();
    if (!slot.has_value()) {
        throw std::runtime_error("Bench is full.");
    }

    const UnitId id = units_.add(std::move(unit));
    Unit* added = units_.get(id);
    // acquireSeq 用来在三合一时保留最新获得的那个单位。
    added->setAcquireSeq(nextAcquireSeq_++);
    added->setEffectiveStats(added->baseStats());
    added->setPlacement(Placement::bench(*slot));
    bench_.setOccupant(*slot, id);
    maybeMergeUnit(id);
    recomputePrepSynergiesAndStats();
    return id;
}

bool GameState::deployFromBench(std::size_t slot, Position target, PlacementPolicy policy) {
    // 从 Bench 部署到棋盘。空格直接放置，占用格只有 Swap 策略才会交换。
    if (phase_ != GamePhase::Prep) {
        return false;
    }

    const auto movingId = bench_.occupant(slot);
    if (!movingId.has_value() || !board_.isInside(target)) {
        return false;
    }

    Unit* moving = units_.get(*movingId);
    if (moving == nullptr || !canOccupyHalf(*moving, target)) {
        return false;
    }

    const auto targetId = board_.occupant(target);
    if (!targetId.has_value()) {
        // 上场人口只在从 Bench 放到棋盘空格时增加。
        if (moving->owner() == Owner::PlayerCtrl && deployedPlayerUnitCount() >= player_.unitCap()) {
            return false;
        }
        bench_.clear(slot);
        board_.setOccupant(target, *movingId);
        moving->setPlacement(Placement::board(target));
        recomputePrepSynergiesAndStats();
        return true;
    }

    Unit* targetUnit = units_.get(*targetId);
    if (policy != PlacementPolicy::Swap || targetUnit == nullptr || targetUnit->owner() != moving->owner()) {
        return false;
    }

    bench_.setOccupant(slot, *targetId);
    targetUnit->setPlacement(Placement::bench(slot));
    board_.setOccupant(target, *movingId);
    moving->setPlacement(Placement::board(target));
    recomputePrepSynergiesAndStats();
    return true;
}

bool GameState::moveBoardUnit(Position from, Position to, PlacementPolicy policy) {
    // 棋盘内移动只允许在 Prep；战斗中的移动由 tickCombat 生成 MoveProposal。
    if (phase_ != GamePhase::Prep) {
        return false;
    }

    const auto movingId = board_.occupant(from);
    if (!movingId.has_value() || !board_.isInside(to)) {
        return false;
    }

    if (from == to) {
        return true;
    }

    Unit* moving = units_.get(*movingId);
    if (moving == nullptr || !canOccupyHalf(*moving, to)) {
        return false;
    }

    const auto targetId = board_.occupant(to);
    if (!targetId.has_value()) {
        board_.clear(from);
        board_.setOccupant(to, *movingId);
        moving->setPlacement(Placement::board(to));
        recomputePrepSynergiesAndStats();
        return true;
    }

    Unit* targetUnit = units_.get(*targetId);
    if (policy != PlacementPolicy::Swap || targetUnit == nullptr || targetUnit->owner() != moving->owner()) {
        return false;
    }

    board_.setOccupant(from, *targetId);
    targetUnit->setPlacement(Placement::board(from));
    board_.setOccupant(to, *movingId);
    moving->setPlacement(Placement::board(to));
    recomputePrepSynergiesAndStats();
    return true;
}

bool GameState::returnToBench(Position from, std::size_t slot) {
    // 只有玩家单位可以回 Bench，敌人永远停留在敌方半场或被清理。
    if (phase_ != GamePhase::Prep) {
        return false;
    }

    const auto movingId = board_.occupant(from);
    if (!movingId.has_value() || !bench_.isEmpty(slot)) {
        return false;
    }

    Unit* moving = units_.get(*movingId);
    if (moving == nullptr || moving->owner() != Owner::PlayerCtrl) {
        return false;
    }

    board_.clear(from);
    bench_.setOccupant(slot, *movingId);
    moving->setPlacement(Placement::bench(slot));
    recomputePrepSynergiesAndStats();
    return true;
}

std::vector<UnitId> GameState::generateEnemiesForRound(int round) {
    // CLI/测试用的显式生成接口；GUI 正式开战时 startCombat 会重新规划敌人。
    clearAllEnemyUnits();
    player_.setCurrentRound(round);
    currentEnemyUnits_ = EncounterGenerator::generate(*this, round);
    return currentEnemyUnits_;
}

ActionResult GameState::startCombat() {
    // Prep -> Combat：冻结当前上场羁绊，记录玩家站位快照，再生成本波敌人。
    if (phase_ != GamePhase::Prep) {
        return {false, "只能在准备阶段开始守家。"};
    }
    if (matchResult_ != MatchResult::Ongoing) {
        return {false, "战局已经结束。"};
    }

    activeSynergies_ = computeSynergiesFromBoard();
    recomputeAllPlayerEffectiveStats();
    const std::vector<UnitId> ar = activePlayerUnits();
    if (ar.empty()) {
        // 没有上场单位时直接进入失败结算，不生成敌人。
        clearAllEnemyUnits();
        playerCombatSnapshot_.clear();
        runtime_.clear();
        lastRoundResult_ = RoundResult::PlayerDefeat;
        phase_ = GamePhase::Resolve;
        return {true, "没有上场植物，本轮失败。"};
    }

    auto spawnPlan = EncounterGenerator::plan(*this, player_.currentRound(), true);
    if (spawnPlan.empty()) {
        return {false, "本波没有可用的僵尸生成位置。"};
    }

    clearAllEnemyUnits();
    playerCombatSnapshot_.clear();
    playerCombatSnapshot_.reserve(ar.size());
    // 快照只记录玩家战前位置，结算时用它把阵容恢复到准备阶段。
    for (UnitId id : ar) {
        const Unit* playerUnit = unit(id);
        const auto position = boardPositionOf(playerUnit);
        if (position.has_value()) {
            playerCombatSnapshot_.push_back({id, *position});
        }
    }

    currentEnemyUnits_.clear();
    currentEnemyUnits_.reserve(spawnPlan.size());
    for (auto& spawn : spawnPlan) {
        currentEnemyUnits_.push_back(addEnemyToBoard(std::move(spawn.unit), spawn.position));
    }

    phase_ = GamePhase::Combat;
    lastRoundResult_ = RoundResult::None;
    initializeRuntime();
    return {true, "僵尸来袭。"};
}

ActionResult GameState::tickCombat() {
    // Combat 的一个 tick：选目标、决定攻击/施法/移动，最后统一应用结果。
    if (phase_ != GamePhase::Combat) {
        return {false, "只能在守家阶段推进战斗。"};
    }

    std::vector<UnitId> active = activeCombatUnits();
    updateCooldowns(active);

    std::vector<CombatEffect> damageEvents;
    std::vector<CombatEffect> healEvents;
    std::vector<MoveProposal> moveProposals;

    // 遍历 active 的快照，不在循环中直接删除单位，避免迭代过程中状态相互污染。
    for (UnitId id : active) {
        Unit* actor = unit(id);
        if (actor == nullptr || !actor->isAlive() || actor->placement().kind != PlacementKind::BoardCell) {
            continue;
        }

        RuntimeState& runtime = runtime_[id];
        const auto targetId = selectTarget(id);
        runtime.currentTarget = targetId;
        if (!targetId.has_value()) {
            actor->setState(UnitState::Idle);
            continue;
        }

        const Unit* target = unit(*targetId);
        if (target == nullptr || !target->isAlive()) {
            actor->setState(UnitState::Idle);
            continue;
        }

        if (isInAttackRange(*actor, *target)) {
            if (runtime.attackCooldown > 0) {
                actor->setState(UnitState::Idle);
                continue;
            }

            runtime.attackCooldown = std::min(combatConfig_.attackInterval, actor->attackInterval());
            if (actor->hasActiveSkill() && actor->skillManaCost() > 0 && actor->mana() >= actor->skillManaCost() &&
                runtime.skillCooldown == 0) {
                // 法力和技能冷却都满足时释放主动技能，技能只向 damage/heal 队列追加效果。
                actor->setState(UnitState::Casting);
                SkillContext context(*this, id, targetId, damageEvents, healEvents);
                actor->castSkill(context);
                actor->setMana(actor->mana() - actor->skillManaCost());
                runtime.skillCooldown = actor->skillCooldownTicks();
            } else {
                // 普攻只造成伤害；法力由每 tick 的自然回复统一处理。
                actor->setState(UnitState::Attacking);
                damageEvents.push_back(CombatEffect{id, *targetId, actor->atk(), false});
            }
            continue;
        }

        if (runtime.moveCooldown > 0) {
            actor->setState(UnitState::Idle);
            continue;
        }

        const auto nextStep = nextStepTowardAttackRange(id, *targetId);
        if (nextStep.has_value()) {
            // 移动先进入提案列表，稍后会统一做冲突检测。
            moveProposals.push_back(MoveProposal{id, *actor->placement().boardCell, *nextStep});
            runtime.moveCooldown = std::min(combatConfig_.moveInterval, actor->moveInterval());
            actor->setState(UnitState::Moving);
        } else {
            actor->setState(UnitState::Idle);
        }
    }

    applyCombatEffects(damageEvents, healEvents);
    clearDeadBoardOccupants();
    applyMoveProposals(moveProposals);
    // 先处理伤害和死亡，再移动，防止单位移动进刚被击杀才腾出的格子时出现顺序歧义。
    checkCombatEnd();

    if (phase_ == GamePhase::Resolve) {
        return {true, "战斗已进入结算。"};
    }
    return {true, "战斗已推进。"};
}

ActionResult GameState::resolveRound() {
    // Resolve -> Prep/GameOver：清敌、恢复玩家站位，再根据胜败发奖励或扣血。
    if (phase_ != GamePhase::Resolve) {
        return {false, "只能在结算阶段完成本波结算。"};
    }

    std::string message;
    clearGeneratedEnemies();
    restorePlayerSnapshot(message);
    runtime_.clear();
    playerCombatSnapshot_.clear();

    if (lastRoundResult_ == RoundResult::PlayerVictory) {
        const int goldReward = combatConfig_.victoryGold + settlementGoldBonusFromSynergies();
        player_.setGold(player_.gold() + goldReward);
        dropVictoryItemIfNeeded();
        if (player_.currentRound() >= combatConfig_.maxRound) {
            matchResult_ = MatchResult::PlayerVictory;
            phase_ = GamePhase::GameOver;
            message += " 战局胜利。";
        } else {
            player_.setCurrentRound(player_.currentRound() + 1);
            phase_ = GamePhase::Prep;
            message += " 本波守住了。";
            recomputePrepSynergiesAndStats();
        }
    } else if (lastRoundResult_ == RoundResult::PlayerDefeat) {
        player_.setGold(player_.gold() + combatConfig_.defeatGold);
        player_.setHp(player_.hp() - combatConfig_.defeatHpLoss);
        if (player_.hp() <= 0) {
            matchResult_ = MatchResult::PlayerDefeat;
            phase_ = GamePhase::GameOver;
            message += " 战局失败。";
        } else {
            phase_ = GamePhase::Prep;
            message += " 本波失守，重试当前波次。";
            recomputePrepSynergiesAndStats();
        }
    } else {
        phase_ = GamePhase::Prep;
        message += " 结算完成，但没有胜负结果。";
        recomputePrepSynergiesAndStats();
    }

    if (phase_ == GamePhase::Prep) {
        generateShopOffersFree();
    }

    return {true, message.empty() ? "本波已结算。" : message};
}

std::vector<UnitId> GameState::activePlayerUnits() const {
    return activeUnitsForOwner(Owner::PlayerCtrl);
}

std::vector<UnitId> GameState::activeEnemyUnits() const {
    return activeUnitsForOwner(Owner::EnemyCtrl);
}

std::vector<UnitId> GameState::activeCombatUnits() const {
    std::vector<UnitId> result = activePlayerUnits();
    const std::vector<UnitId> enemies = activeEnemyUnits();
    result.insert(result.end(), enemies.begin(), enemies.end());
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

const std::vector<UnitId>& GameState::currentEnemyUnits() const {
    return currentEnemyUnits_;
}

bool GameState::canOccupyHalf(const Unit& unit, Position position) const {
    // 半场规则集中在这里，部署、移动和读档校验都复用同一判断。
    if (!board_.isInside(position)) {
        return false;
    }
    if (unit.owner() == Owner::PlayerCtrl) {
        return board_.isPlayerHalf(position);
    }
    return board_.isEnemyHalf(position);
}

UnitId GameState::addEnemyToBoard(std::unique_ptr<Unit> unit, Position position) {
    // 敌人直接加入棋盘，不经过 Bench。
    if (!unit || unit->owner() != Owner::EnemyCtrl) {
        throw std::invalid_argument("Enemy spawns must be EnemyCtrl units.");
    }
    if (!board_.canPlace(*unit, position)) {
        throw std::runtime_error("Invalid enemy spawn position.");
    }

    const UnitId id = units_.add(std::move(unit));
    Unit* added = units_.get(id);
    board_.setOccupant(position, id);
    added->setPlacement(Placement::board(position));
    return id;
}

void GameState::clearGeneratedEnemies() {
    // 只清理 currentEnemyUnits_ 记录的本波敌人，用于正常结算。
    for (UnitId id : currentEnemyUnits_) {
        Unit* enemy = units_.get(id);
        if (enemy != nullptr && enemy->placement().kind == PlacementKind::BoardCell) {
            board_.clear(*enemy->placement().boardCell);
        }
        units_.remove(id);
    }
    currentEnemyUnits_.clear();
}

void GameState::clearAllEnemyUnits() {
    // 更强的清理函数，用于重新生成波次前确保棋盘没有残留敌人。
    const std::vector<UnitId> ids = units_.ids();
    for (UnitId id : ids) {
        Unit* enemy = units_.get(id);
        if (enemy == nullptr || enemy->owner() != Owner::EnemyCtrl) {
            continue;
        }
        if (enemy->placement().kind == PlacementKind::BoardCell) {
            board_.clear(*enemy->placement().boardCell);
        }
        units_.remove(id);
    }
    currentEnemyUnits_.clear();
}

void GameState::clearBoardOccupant(Position position) {
    const auto occupant = board_.occupant(position);
    if (occupant.has_value()) {
        Unit* unitOnCell = units_.get(*occupant);
        if (unitOnCell != nullptr) {
            unitOnCell->setPlacement(Placement::none());
        }
    }
    board_.clear(position);
}

void GameState::clearBoardForSettlement() {
    // 结算恢复前先清空棋盘占用，随后按玩家快照重新放回单位。
    for (int row = 0; row < board_.rows(); ++row) {
        for (int col = 0; col < board_.cols(); ++col) {
            const Position position{row, col};
            if (board_.occupant(position).has_value()) {
                clearBoardOccupant(position);
            }
        }
    }
}

void GameState::initializeRuntime() {
    runtime_.clear();
    // 开战时给每个单位一个初始攻击冷却，避免所有单位第一帧同时出手。
    for (UnitId id : activeCombatUnits()) {
        Unit* combatUnit = unit(id);
        const int attackInterval = combatUnit != nullptr ? std::min(combatConfig_.attackInterval, combatUnit->attackInterval())
                                                         : combatConfig_.attackInterval;
        if (combatUnit != nullptr) {
            combatUnit->setMana(combatUnit->initialMana());
        }
        RuntimeState state;
        state.attackCooldown = attackInterval;
        runtime_[id] = state;
    }
}

void GameState::updateCooldowns(const std::vector<UnitId>& activeUnits) {
    for (UnitId id : activeUnits) {
        RuntimeState& runtime = runtime_[id];
        Unit* activeUnit = unit(id);
        if (runtime.attackCooldown > 0) {
            --runtime.attackCooldown;
        }
        if (runtime.moveCooldown > 0) {
            --runtime.moveCooldown;
        }
        if (runtime.skillCooldown > 0) {
            --runtime.skillCooldown;
        }
        if (activeUnit != nullptr && activeUnit->isAlive() && activeUnit->manaRegenPerSecond() > 0) {
            runtime.manaRegenRemainder += activeUnit->manaRegenPerSecond();
            const int gainedMana = runtime.manaRegenRemainder / 60;
            runtime.manaRegenRemainder %= 60;
            if (gainedMana > 0) {
                activeUnit->setMana(activeUnit->mana() + gainedMana);
            }
        }
    }
}

std::optional<UnitId> GameState::selectTarget(UnitId attackerId) const {
    const Unit* attacker = unit(attackerId);
    const auto attackerPosition = boardPositionOf(attacker);
    if (attacker == nullptr || !attacker->isAlive() || !attackerPosition.has_value()) {
        return std::nullopt;
    }

    const Owner targetOwner = opposingOwner(attacker->owner());
    const std::vector<UnitId> candidates = targetOwner == Owner::PlayerCtrl ? activePlayerUnits() : activeEnemyUnits();

    std::optional<UnitId> bestId;
    int bestScore = 0;
    int bestBaseThreat = 0;
    int bestDistSq = 0;
    int bestHp = 0;
    Position bestPosition;
    for (UnitId candidateId : candidates) {
        const Unit* candidate = unit(candidateId);
        const auto candidatePosition = boardPositionOf(candidate);
        if (candidate == nullptr || !candidate->isAlive() || !candidatePosition.has_value()) {
            continue;
        }

        const int distSq = distanceSquared(*attackerPosition, *candidatePosition);
        const int baseThreat = targetBaseThreat(*candidate);
        const auto runtimeIt = runtime_.find(candidateId);
        const int candidateSkillCooldown = runtimeIt == runtime_.end() ? 0 : runtimeIt->second.skillCooldown;
        const int score = baseThreat + targetSkillThreat(*candidate, candidateSkillCooldown) +
                          executeValue(*attacker, *candidate) - distSq * 3;
        // 索敌从单纯“最近优先”改成“威胁分 + 距离惩罚”：
        // 先打输出高、技能快好、可斩杀的目标；分数相同时再用距离、血量、位置和 ID 保持确定性。
        const bool better = !bestId.has_value() || score > bestScore ||
                            (score == bestScore && baseThreat > bestBaseThreat) ||
                            (score == bestScore && baseThreat == bestBaseThreat && distSq < bestDistSq) ||
                            (score == bestScore && baseThreat == bestBaseThreat && distSq == bestDistSq &&
                             candidate->hp() < bestHp) ||
                            (score == bestScore && baseThreat == bestBaseThreat && distSq == bestDistSq &&
                             candidate->hp() == bestHp && candidatePosition->col < bestPosition.col) ||
                            (score == bestScore && baseThreat == bestBaseThreat && distSq == bestDistSq &&
                             candidate->hp() == bestHp && candidatePosition->col == bestPosition.col &&
                             candidatePosition->row > bestPosition.row) ||
                            (score == bestScore && baseThreat == bestBaseThreat && distSq == bestDistSq &&
                             candidate->hp() == bestHp && candidatePosition->col == bestPosition.col &&
                             candidatePosition->row == bestPosition.row && candidateId < *bestId);
        if (better) {
            bestId = candidateId;
            bestScore = score;
            bestBaseThreat = baseThreat;
            bestDistSq = distSq;
            bestHp = candidate->hp();
            bestPosition = *candidatePosition;
        }
    }

    return bestId;
}

std::optional<Position> GameState::nextStepTowardAttackRange(UnitId attackerId, UnitId targetId) const {
    const Unit* attacker = unit(attackerId);
    const Unit* target = unit(targetId);
    const auto start = boardPositionOf(attacker);
    const auto targetPosition = boardPositionOf(target);
    if (attacker == nullptr || target == nullptr || !start.has_value() || !targetPosition.has_value()) {
        return std::nullopt;
    }
    if (isInAttackRange(*attacker, *target)) {
        return std::nullopt;
    }

    const int rangeSq = attacker->range() * attacker->range();
    auto isGoal = [&](Position position) {
        // 目标格不是敌人所在格，而是能站住且进入攻击范围的空格。
        return position != *targetPosition && board_.isEmpty(position) &&
               distanceSquared(position, *targetPosition) <= rangeSq;
    };

    struct Node {
        Position position;
        Position firstStep;
    };

    std::queue<Node> frontier;
    std::vector<bool> visited(static_cast<std::size_t>(board_.rows() * board_.cols()), false);
    visited[static_cast<std::size_t>(positionKey(board_, *start))] = true;

    // 广度优先搜索四方向最短路；Node 记录 firstStep，最终只返回下一步。
    const std::vector<Position> directions{{-1, 0}, {0, -1}, {0, 1}, {1, 0}};
    for (const Position& direction : directions) {
        const Position next{start->row + direction.row, start->col + direction.col};
        if (!board_.isInside(next) || !board_.isEmpty(next)) {
            continue;
        }
        visited[static_cast<std::size_t>(positionKey(board_, next))] = true;
        if (isGoal(next)) {
            return next;
        }
        frontier.push(Node{next, next});
    }

    while (!frontier.empty()) {
        const Node current = frontier.front();
        frontier.pop();

        for (const Position& direction : directions) {
            const Position next{current.position.row + direction.row, current.position.col + direction.col};
            if (!board_.isInside(next) || !board_.isEmpty(next)) {
                continue;
            }
            const auto key = static_cast<std::size_t>(positionKey(board_, next));
            if (visited[key]) {
                continue;
            }
            visited[key] = true;
            if (isGoal(next)) {
                return current.firstStep;
            }
            frontier.push(Node{next, current.firstStep});
        }
    }

    return std::nullopt;
}

bool GameState::isInAttackRange(const Unit& attacker, const Unit& target) const {
    const auto attackerPosition = boardPositionOf(&attacker);
    const auto targetPosition = boardPositionOf(&target);
    if (!attackerPosition.has_value() || !targetPosition.has_value()) {
        return false;
    }
    const int rangeSq = attacker.range() * attacker.range();
    return distanceSquared(*attackerPosition, *targetPosition) <= rangeSq;
}

void GameState::applyCombatEffects(const std::vector<CombatEffect>& damageEvents,
                                   const std::vector<CombatEffect>& healEvents) {
    std::unordered_map<UnitId, int> pendingHp;
    // pendingHp 保存本 tick 的临时生命，多个伤害/治疗合并后再写回 Unit。
    auto hpFor = [&](UnitId id) -> int& {
        auto it = pendingHp.find(id);
        if (it == pendingHp.end()) {
            const Unit* target = unit(id);
            it = pendingHp.emplace(id, target != nullptr ? target->hp() : 0).first;
        }
        return it->second;
    };

    for (const CombatEffect& effect : damageEvents) {
        Unit* target = unit(effect.target);
        if (target == nullptr || !target->isAlive()) {
            continue;
        }

        int& hp = hpFor(effect.target);
        hp = std::max(0, hp - effect.amount);

    }

    for (const CombatEffect& effect : healEvents) {
        Unit* target = unit(effect.target);
        if (target == nullptr || target->state() == UnitState::Dead) {
            continue;
        }

        int& hp = hpFor(effect.target);
        int amount = effect.amount;
        const Unit* source = unit(effect.source);
        if (source != nullptr && ownerHasActiveSynergy(source->owner(), "healer")) {
            // healer 羁绊在结算治疗时统一放大治疗量。
            amount = amount * 125 / 100;
        }
        hp = std::min(target->maxHp(), hp + amount);
    }

    for (const auto& [id, hp] : pendingHp) {
        Unit* target = unit(id);
        if (target != nullptr) {
            target->setHp(hp);
        }
    }
}

void GameState::applyMoveProposals(const std::vector<MoveProposal>& proposals) {
    std::unordered_map<int, UnitId> chosenMoverByTarget;
    for (const MoveProposal& proposal : proposals) {
        const int targetKey = positionKey(board_, proposal.to);
        auto [it, inserted] = chosenMoverByTarget.emplace(targetKey, proposal.unitId);
        if (!inserted) {
            it->second = std::min(it->second, proposal.unitId);
        }
    }

    for (const MoveProposal& proposal : proposals) {
        const auto chosen = chosenMoverByTarget.find(positionKey(board_, proposal.to));
        if (chosen == chosenMoverByTarget.end() || chosen->second != proposal.unitId) {
            Unit* blocked = unit(proposal.unitId);
            if (blocked != nullptr && blocked->isAlive() && blocked->state() == UnitState::Moving) {
                blocked->setState(UnitState::Idle);
            }
            continue;
        }

        Unit* moving = unit(proposal.unitId);
        if (moving == nullptr || !moving->isAlive() || moving->placement().kind != PlacementKind::BoardCell) {
            continue;
        }
        if (*moving->placement().boardCell != proposal.from) {
            continue;
        }
        if (board_.occupant(proposal.from) != proposal.unitId || !board_.isInside(proposal.to) ||
            !board_.isEmpty(proposal.to)) {
            continue;
        }

        board_.clear(proposal.from);
        board_.setOccupant(proposal.to, proposal.unitId);
        moving->setPlacement(Placement::board(proposal.to));
    }
}

void GameState::clearDeadBoardOccupants() {
    // 死亡单位仍保留在 UnitManager 中一小段时间，但先从棋盘占用表移除。
    for (int row = 0; row < board_.rows(); ++row) {
        for (int col = 0; col < board_.cols(); ++col) {
            const Position position{row, col};
            const auto occupant = board_.occupant(position);
            if (!occupant.has_value()) {
                continue;
            }

            Unit* unitOnCell = unit(*occupant);
            if (unitOnCell == nullptr || !unitOnCell->isAlive()) {
                if (unitOnCell != nullptr) {
                    unitOnCell->setState(UnitState::Dead);
                    unitOnCell->setPlacement(Placement::none());
                }
                board_.clear(position);
            }
        }
    }
}

void GameState::checkCombatEnd() {
    // 任一阵营没有存活棋盘单位时，战斗立即进入 Resolve。
    if (phase_ != GamePhase::Combat) {
        return;
    }

    if (activeEnemyUnits().empty()) {
        phase_ = GamePhase::Resolve;
        lastRoundResult_ = RoundResult::PlayerVictory;
        return;
    }
    if (activePlayerUnits().empty()) {
        phase_ = GamePhase::Resolve;
        lastRoundResult_ = RoundResult::PlayerDefeat;
    }
}

void GameState::restorePlayerSnapshot(std::string& message) {
    // 战斗中玩家单位可能移动或死亡，结算后统一回到战前站位并满血空蓝。
    clearBoardForSettlement();
    for (const auto& [id, position] : playerCombatSnapshot_) {
        Unit* playerUnit = unit(id);
        if (playerUnit == nullptr) {
            std::ostringstream out;
            out << " Snapshot unit #" << id << " was missing.";
            message += out.str();
            continue;
        }

        playerUnit->setHp(playerUnit->maxHp());
        playerUnit->setMana(0);
        playerUnit->setState(UnitState::Idle);
        playerUnit->setPlacement(Placement::board(position));
        board_.setOccupant(position, id);
    }
}

void GameState::generateShopOffersFree() {
    // 随机生成 5 个商店槽位；刷新成本在 refreshShop 中扣除。
    shopOffers_.assign(5, std::nullopt);
    const auto& definitions = unitCatalog();
    if (definitions.empty()) {
        return;
    }
    std::uniform_int_distribution<std::size_t> distribution(0, definitions.size() - 1);
    for (std::optional<ShopOffer>& slot : shopOffers_) {
        const UnitDefinition& definition = definitions[distribution(rng_)];
        slot = ShopOffer{definition.definitionId, definition.cost};
    }
}

void GameState::recomputePrepSynergiesAndStats() {
    // 准备/结算/结束阶段可以即时刷新羁绊；战斗中羁绊保持开战时快照。
    if (phase_ == GamePhase::Prep || phase_ == GamePhase::Resolve || phase_ == GamePhase::GameOver) {
        activeSynergies_ = computeSynergiesFromBoard();
        recomputeAllPlayerEffectiveStats();
    }
}

std::vector<SynergyStatus> GameState::computeSynergiesFromBoard() const {
    std::unordered_map<std::string, int> contributors;
    // 羁绊按场上的单位实例计数；二星单位仍是一个单位，但同名单位重复上场会分别贡献。
    for (int row = 0; row < board_.rows(); ++row) {
        for (int col = 0; col < board_.cols(); ++col) {
            const auto occupant = board_.occupant(Position{row, col});
            if (!occupant.has_value()) {
                continue;
            }
            const Unit* boardUnit = unit(*occupant);
            if (boardUnit == nullptr || boardUnit->owner() != Owner::PlayerCtrl) {
                continue;
            }
            for (const std::string& trait : boardUnit->traits()) {
                ++contributors[trait];
            }
        }
    }

    auto makeStatus = [&](std::string trait,
                          std::array<int, 2> thresholds,
                          std::string lowEffect,
                          std::string highEffect) {
        // 每个羁绊有低/高两档阈值，未激活时记录 nextThreshold 供 GUI 展示。
        const int count = contributors[trait];
        SynergyStatus status;
        status.trait = std::move(trait);
        status.count = count;
        if (count >= thresholds[1]) {
            status.active = true;
            status.activeThreshold = thresholds[1];
            status.nextThreshold = 0;
            status.effectDescription = std::move(highEffect);
        } else if (count >= thresholds[0]) {
            status.active = true;
            status.activeThreshold = thresholds[0];
            status.nextThreshold = thresholds[1];
            status.effectDescription = std::move(lowEffect);
        } else {
            status.active = false;
            status.activeThreshold = 0;
            status.nextThreshold = thresholds[0];
            status.effectDescription = "Inactive";
        }
        return status;
    };

    std::vector<SynergyStatus> result;
    result.push_back(makeStatus("shooter", {2, 4}, "射手攻击间隔 -10%", "射手攻击间隔 -20%"));
    result.push_back(makeStatus("nut", {2, 4}, "坚果最大生命 +200", "坚果最大生命 +450"));
    result.push_back(makeStatus("sun", {2, 3}, "胜利结算阳光 +1", "胜利结算阳光 +2"));
    result.push_back(makeStatus("healer", {2, 2}, "治疗效果 +25%", "治疗效果 +25%"));
    result.push_back(makeStatus("fungus", {2, 4}, "真菌技能消耗 -10", "真菌技能消耗 -25"));
    result.push_back(makeStatus("spike", {2, 2}, "地刺攻击力 +15", "地刺攻击力 +15"));
    return result;
}

void GameState::recomputeAllPlayerEffectiveStats() {
    for (UnitId id : units_.ids()) {
        Unit* candidate = unit(id);
        if (candidate != nullptr && candidate->owner() == Owner::PlayerCtrl) {
            computeEffectiveStats(id);
        }
    }
}

void GameState::maybeMergeUnit(UnitId newestId) {
    // 三个同 definitionId 的一星玩家单位自动合成一个二星单位。
    if (phase_ != GamePhase::Prep) {
        return;
    }
    Unit* newest = unit(newestId);
    if (newest == nullptr || newest->owner() != Owner::PlayerCtrl || newest->star() != 1) {
        return;
    }

    std::vector<UnitId> matches;
    for (UnitId id : units_.ids()) {
        Unit* candidate = unit(id);
        if (candidate == nullptr || candidate->owner() != Owner::PlayerCtrl || candidate->star() != newest->star() ||
            candidate->definitionId() != newest->definitionId()) {
            continue;
        }
        if (candidate->placement().kind == PlacementKind::BenchSlot ||
            candidate->placement().kind == PlacementKind::BoardCell) {
            matches.push_back(id);
        }
    }
    if (matches.size() < 3) {
        return;
    }

    std::sort(matches.begin(), matches.end(), [&](UnitId lhs, UnitId rhs) {
        const Unit* left = unit(lhs);
        const Unit* right = unit(rhs);
        // acquireSeq 越大表示越新，合成时保留最新购买或添加的那个单位。
        return left->acquireSeq() > right->acquireSeq();
    });

    const UnitId keepId = matches.front();
    Unit* keep = unit(keepId);
    keep->setStar(2);
    for (std::size_t i = 1; i < 3; ++i) {
        Unit* removed = unit(matches[i]);
        if (removed == nullptr) {
            continue;
        }
        returnEquippedItemToInventory(*removed);
        // 被消耗单位的装备回库存，避免合成吞掉装备实例。
        removeUnitFromPlacement(matches[i]);
        units_.remove(matches[i]);
    }
    computeEffectiveStats(keepId);
}

void GameState::removeUnitFromPlacement(UnitId id) {
    Unit* removed = unit(id);
    if (removed == nullptr) {
        return;
    }
    const Placement placement = removed->placement();
    if (placement.kind == PlacementKind::BenchSlot && placement.benchSlot.has_value()) {
        bench_.clear(*placement.benchSlot);
    } else if (placement.kind == PlacementKind::BoardCell && placement.boardCell.has_value()) {
        board_.clear(*placement.boardCell);
    }
    removed->setPlacement(Placement::none());
}

void GameState::returnEquippedItemToInventory(Unit& unit) {
    if (!unit.equippedItemId().has_value()) {
        return;
    }
    equipmentInventory_.push_back(*unit.equippedItemId());
    unit.setEquippedItemId(std::nullopt);
    std::sort(equipmentInventory_.begin(), equipmentInventory_.end());
}

bool GameState::hasItemInInventory(ItemId itemId) const {
    return std::find(equipmentInventory_.begin(), equipmentInventory_.end(), itemId) != equipmentInventory_.end();
}

const ItemInstance* GameState::itemInstance(ItemId itemId) const {
    const auto it = itemInstances_.find(itemId);
    return it == itemInstances_.end() ? nullptr : &it->second;
}

ItemInstance* GameState::itemInstance(ItemId itemId) {
    const auto it = itemInstances_.find(itemId);
    return it == itemInstances_.end() ? nullptr : &it->second;
}

void GameState::dropVictoryItemIfNeeded() {
    const int dropPercent = std::clamp(combatConfig_.itemDropPercent, 0, 100);
    if (dropPercent <= 0) {
        return;
    }
    bool shouldDrop = dropPercent >= 100;
    if (!shouldDrop) {
        std::uniform_int_distribution<int> distribution(1, 100);
        shouldDrop = distribution(rng_) <= dropPercent;
    }
    if (!shouldDrop) {
        return;
    }
    const auto& definitions = itemCatalog();
    if (definitions.empty()) {
        return;
    }
    // 掉落按当前波次轮换装备目录，随机性只决定是否掉落。
    const std::size_t index = static_cast<std::size_t>((player_.currentRound() - 1) % static_cast<int>(definitions.size()));
    addItemToInventory(definitions[index].itemDefId);
}

int GameState::settlementGoldBonusFromSynergies() const {
    // sun 羁绊只在胜利结算时提供额外金币。
    for (const SynergyStatus& synergy : activeSynergies_) {
        if (synergy.trait == "sun" && synergy.active) {
            return synergy.activeThreshold >= 3 ? 2 : 1;
        }
    }
    return 0;
}

bool GameState::ownerHasActiveSynergy(Owner owner, const std::string& trait) const {
    if (owner != Owner::PlayerCtrl) {
        return false;
    }
    return std::any_of(activeSynergies_.begin(), activeSynergies_.end(), [&](const SynergyStatus& status) {
        return status.trait == trait && status.active;
    });
}

void GameState::rebuildOccupancyFromUnitPlacements() {
    // 读档后以 Unit::placement 为真相重建 Board/Bench 占用表。
    for (int row = 0; row < board_.rows(); ++row) {
        for (int col = 0; col < board_.cols(); ++col) {
            board_.clear(Position{row, col});
        }
    }
    for (std::size_t slot = 0; slot < bench_.capacity(); ++slot) {
        bench_.clear(slot);
    }

    for (UnitId id : units_.ids()) {
        Unit* placed = unit(id);
        if (placed == nullptr) {
            continue;
        }
        const Placement& placement = placed->placement();
        if (placement.kind == PlacementKind::BenchSlot && placement.benchSlot.has_value()) {
            bench_.setOccupant(*placement.benchSlot, id);
        } else if (placement.kind == PlacementKind::BoardCell && placement.boardCell.has_value()) {
            board_.setOccupant(*placement.boardCell, id);
        }
    }
}

void GameState::clearRuntimeOnlyState() {
    // 冷却、当前敌人列表等只服务战斗运行，不应跨读档或阶段残留。
    runtime_.clear();
    currentEnemyUnits_.clear();
    if (phase_ != GamePhase::Resolve) {
        playerCombatSnapshot_.clear();
    }
}

std::vector<UnitId> GameState::activeUnitsForOwner(Owner owner) const {
    std::vector<UnitId> result;
    // 只扫描棋盘，不扫描 Bench；Bench 单位不参与战斗。
    for (int row = 0; row < board_.rows(); ++row) {
        for (int col = 0; col < board_.cols(); ++col) {
            const Position position{row, col};
            const auto id = board_.occupant(position);
            if (!id.has_value()) {
                continue;
            }

            const Unit* candidate = units_.get(*id);
            if (candidate != nullptr && candidate->owner() == owner && candidate->isAlive()) {
                result.push_back(*id);
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

}  // namespace synera
