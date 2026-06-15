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
    const int dr = a.row - b.row;
    const int dc = a.col - b.col;
    return dr * dr + dc * dc;
}

int positionKey(const Board& board, Position position) {
    return position.row * board.cols() + position.col;
}

Owner opposingOwner(Owner owner) {
    return owner == Owner::PlayerCtrl ? Owner::EnemyCtrl : Owner::PlayerCtrl;
}

std::optional<Position> boardPositionOf(const Unit* unit) {
    if (unit == nullptr || unit->placement().kind != PlacementKind::BoardCell) {
        return std::nullopt;
    }
    return unit->placement().boardCell;
}

bool hasTrait(const Unit& unit, const std::string& trait) {
    const auto& traits = unit.traits();
    return std::find(traits.begin(), traits.end(), trait) != traits.end();
}

int percentAdjusted(int value, int percentDelta) {
    return std::max(1, value + (value * percentDelta) / 100);
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
    const ItemId itemId = nextItemId_++;
    itemInstances_.emplace(itemId, ItemInstance{itemId, itemDefId});
    equipmentInventory_.push_back(itemId);
    return itemId;
}

ActionResult GameState::refreshShop() {
    if (phase_ != GamePhase::Prep) {
        return {false, "refreshShop is only valid during Prep."};
    }
    constexpr int refreshCost = 2;
    if (player_.gold() < refreshCost) {
        return {false, "Not enough gold to refresh shop."};
    }
    player_.setGold(player_.gold() - refreshCost);
    generateShopOffersFree();
    return {true, "Shop refreshed."};
}

ActionResult GameState::purchaseShopSlot(std::size_t index) {
    if (phase_ != GamePhase::Prep) {
        return {false, "purchaseShopSlot is only valid during Prep."};
    }
    if (index >= shopOffers_.size()) {
        return {false, "Shop slot index is invalid."};
    }
    if (!shopOffers_[index].has_value()) {
        return {false, "Shop slot is empty."};
    }

    const ShopOffer offer = *shopOffers_[index];
    const UnitDefinition* definition = findUnitDefinition(offer.definitionId);
    if (definition == nullptr) {
        return {false, "Shop offer references an unknown unit."};
    }
    if (player_.gold() < offer.cost) {
        return {false, "Not enough gold to purchase unit."};
    }
    if (!bench_.firstEmptySlot().has_value()) {
        return {false, "Bench is full."};
    }

    player_.setGold(player_.gold() - offer.cost);
    std::unique_ptr<Unit> unit = createUnitFromDefinition(*definition, Owner::PlayerCtrl);
    const UnitId id = addUnitToBench(std::move(unit));
    maybeMergeUnit(id);
    shopOffers_[index].reset();
    recomputePrepSynergiesAndStats();
    return {true, "Unit purchased."};
}

ActionResult GameState::upgradePopulation() {
    if (phase_ != GamePhase::Prep) {
        return {false, "upgradePopulation is only valid during Prep."};
    }
    constexpr int maxLevel = 6;
    constexpr int maxUnitCap = 8;
    if (player_.level() >= maxLevel) {
        return {false, "Population level is already at maximum."};
    }
    const int cost = 4 + 2 * (player_.level() - 1);
    if (player_.gold() < cost) {
        return {false, "Not enough gold to upgrade population."};
    }

    player_.setGold(player_.gold() - cost);
    player_.setLevel(player_.level() + 1);
    player_.setUnitCap(std::min(maxUnitCap, player_.unitCap() + 1));
    return {true, "Population upgraded."};
}

ActionResult GameState::equipItem(ItemId itemId, UnitId unitId) {
    if (phase_ != GamePhase::Prep) {
        return {false, "equipItem is only valid during Prep."};
    }
    if (!hasItemInInventory(itemId)) {
        return {false, "Item is not in inventory."};
    }
    Unit* target = unit(unitId);
    if (target == nullptr || target->owner() != Owner::PlayerCtrl) {
        return {false, "Only player units can equip items."};
    }
    if (target->equippedItemId().has_value()) {
        return {false, "Unit already has an item."};
    }

    equipmentInventory_.erase(std::remove(equipmentInventory_.begin(), equipmentInventory_.end(), itemId),
                              equipmentInventory_.end());
    target->setEquippedItemId(itemId);
    computeEffectiveStats(unitId);
    recomputePrepSynergiesAndStats();
    return {true, "Item equipped."};
}

UnitStats GameState::computeEffectiveStats(UnitId id) {
    Unit* target = unit(id);
    if (target == nullptr) {
        return {};
    }

    UnitStats stats = target->baseStats();
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
        if (synergy.trait == "Warrior") {
            stats.atk += synergy.activeThreshold >= 4 ? 25 : 10;
        } else if (synergy.trait == "Guardian") {
            stats.maxHp += synergy.activeThreshold >= 4 ? 250 : 100;
        } else if (synergy.trait == "Mystic") {
            stats.maxMana = std::max(10, stats.maxMana + (synergy.activeThreshold >= 3 ? -20 : -10));
        } else if (synergy.trait == "Ranger") {
            stats.attackInterval = percentAdjusted(stats.attackInterval, -10);
        }
    }

    target->setEffectiveStats(stats);
    return target->effectiveStats();
}

ActionResult GameState::saveToFile(const std::string& path) const {
    if (phase_ == GamePhase::Combat) {
        return {false, "Saving during Combat is not supported."};
    }

    std::ofstream out(path);
    if (!out) {
        return {false, "Failed to open save file for writing."};
    }

    out << "SYNERA_SAVE 1\n";
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
        return {false, "Failed to open save file for reading."};
    }

    auto fail = [](const std::string& message) { return ActionResult{false, message}; };
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
    added->setAcquireSeq(nextAcquireSeq_++);
    added->setEffectiveStats(added->baseStats());
    added->setPlacement(Placement::bench(*slot));
    bench_.setOccupant(*slot, id);
    maybeMergeUnit(id);
    recomputePrepSynergiesAndStats();
    return id;
}

bool GameState::deployFromBench(std::size_t slot, Position target, PlacementPolicy policy) {
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
    clearAllEnemyUnits();
    player_.setCurrentRound(round);
    currentEnemyUnits_ = EncounterGenerator::generate(*this, round);
    return currentEnemyUnits_;
}

ActionResult GameState::startCombat() {
    if (phase_ != GamePhase::Prep) {
        return {false, "startCombat is only valid during Prep."};
    }
    if (matchResult_ != MatchResult::Ongoing) {
        return {false, "The match has already ended."};
    }

    activeSynergies_ = computeSynergiesFromBoard();
    recomputeAllPlayerEffectiveStats();
    const std::vector<UnitId> ar = activePlayerUnits();
    if (ar.empty()) {
        clearAllEnemyUnits();
        playerCombatSnapshot_.clear();
        runtime_.clear();
        lastRoundResult_ = RoundResult::PlayerDefeat;
        phase_ = GamePhase::Resolve;
        return {true, "Player defeated: no active units."};
    }

    auto spawnPlan = EncounterGenerator::plan(*this, player_.currentRound(), true);
    if (spawnPlan.empty()) {
        return {false, "No valid enemy spawn positions for this round."};
    }

    clearAllEnemyUnits();
    playerCombatSnapshot_.clear();
    playerCombatSnapshot_.reserve(ar.size());
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
    return {true, "Combat started."};
}

ActionResult GameState::tickCombat() {
    if (phase_ != GamePhase::Combat) {
        return {false, "tickCombat is only valid during Combat."};
    }

    std::vector<UnitId> active = activeCombatUnits();
    updateCooldowns(active);

    std::vector<CombatEffect> damageEvents;
    std::vector<CombatEffect> healEvents;
    std::vector<MoveProposal> moveProposals;

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
            if (actor->maxMana() > 0 && actor->mana() >= actor->maxMana()) {
                actor->setState(UnitState::Casting);
                SkillContext context(*this, id, targetId, damageEvents, healEvents);
                actor->castSkill(context);
                actor->setMana(0);
            } else {
                actor->setState(UnitState::Attacking);
                damageEvents.push_back(CombatEffect{id, *targetId, actor->atk(), true});
            }
            continue;
        }

        if (runtime.moveCooldown > 0) {
            actor->setState(UnitState::Idle);
            continue;
        }

        const auto nextStep = nextStepTowardAttackRange(id, *targetId);
        if (nextStep.has_value()) {
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
    checkCombatEnd();

    if (phase_ == GamePhase::Resolve) {
        return {true, "Combat resolved."};
    }
    return {true, "Combat tick advanced."};
}

ActionResult GameState::resolveRound() {
    if (phase_ != GamePhase::Resolve) {
        return {false, "resolveRound is only valid during Resolve."};
    }

    std::string message;
    clearGeneratedEnemies();
    restorePlayerSnapshot(message);
    runtime_.clear();
    playerCombatSnapshot_.clear();

    if (lastRoundResult_ == RoundResult::PlayerVictory) {
        player_.setGold(player_.gold() + combatConfig_.victoryGold);
        dropVictoryItemIfNeeded();
        if (player_.currentRound() >= combatConfig_.maxRound) {
            matchResult_ = MatchResult::PlayerVictory;
            phase_ = GamePhase::GameOver;
            message += " Match won.";
        } else {
            player_.setCurrentRound(player_.currentRound() + 1);
            phase_ = GamePhase::Prep;
            message += " Round won.";
            recomputePrepSynergiesAndStats();
        }
    } else if (lastRoundResult_ == RoundResult::PlayerDefeat) {
        player_.setGold(player_.gold() + combatConfig_.defeatGold);
        player_.setHp(player_.hp() - combatConfig_.defeatHpLoss);
        if (player_.hp() <= 0) {
            matchResult_ = MatchResult::PlayerDefeat;
            phase_ = GamePhase::GameOver;
            message += " Match lost.";
        } else {
            phase_ = GamePhase::Prep;
            message += " Round lost. Retry current round.";
            recomputePrepSynergiesAndStats();
        }
    } else {
        phase_ = GamePhase::Prep;
        message += " Resolve completed without a round result.";
        recomputePrepSynergiesAndStats();
    }

    return {true, message.empty() ? "Round resolved." : message};
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
    if (!board_.isInside(position)) {
        return false;
    }
    if (unit.owner() == Owner::PlayerCtrl) {
        return board_.isPlayerHalf(position);
    }
    return board_.isEnemyHalf(position);
}

UnitId GameState::addEnemyToBoard(std::unique_ptr<Unit> unit, Position position) {
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
    for (UnitId id : activeCombatUnits()) {
        const Unit* combatUnit = unit(id);
        const int attackInterval = combatUnit != nullptr ? std::min(combatConfig_.attackInterval, combatUnit->attackInterval())
                                                         : combatConfig_.attackInterval;
        runtime_[id] = RuntimeState{attackInterval, 0, std::nullopt};
    }
}

void GameState::updateCooldowns(const std::vector<UnitId>& activeUnits) {
    for (UnitId id : activeUnits) {
        RuntimeState& runtime = runtime_[id];
        if (runtime.attackCooldown > 0) {
            --runtime.attackCooldown;
        }
        if (runtime.moveCooldown > 0) {
            --runtime.moveCooldown;
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
        const bool better = !bestId.has_value() || distSq < bestDistSq ||
                            (distSq == bestDistSq && candidate->hp() > bestHp) ||
                            (distSq == bestDistSq && candidate->hp() == bestHp &&
                             candidatePosition->col < bestPosition.col) ||
                            (distSq == bestDistSq && candidate->hp() == bestHp &&
                             candidatePosition->col == bestPosition.col &&
                             candidatePosition->row > bestPosition.row) ||
                            (distSq == bestDistSq && candidate->hp() == bestHp &&
                             candidatePosition->col == bestPosition.col &&
                             candidatePosition->row == bestPosition.row && candidateId < *bestId);
        if (better) {
            bestId = candidateId;
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

        if (effect.grantsMana) {
            Unit* source = unit(effect.source);
            if (source != nullptr && source->isAlive()) {
                source->setMana(source->mana() + combatConfig_.manaPerAttack);
            }
        }
    }

    for (const CombatEffect& effect : healEvents) {
        Unit* target = unit(effect.target);
        if (target == nullptr || target->state() == UnitState::Dead) {
            continue;
        }

        int& hp = hpFor(effect.target);
        int amount = effect.amount;
        const Unit* source = unit(effect.source);
        if (source != nullptr && ownerHasActiveSynergy(source->owner(), "Healer")) {
            amount = amount * 120 / 100;
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
    std::unordered_map<int, int> proposalCounts;
    for (const MoveProposal& proposal : proposals) {
        ++proposalCounts[positionKey(board_, proposal.to)];
    }

    for (const MoveProposal& proposal : proposals) {
        if (proposalCounts[positionKey(board_, proposal.to)] != 1) {
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
    if (phase_ == GamePhase::Prep || phase_ == GamePhase::Resolve || phase_ == GamePhase::GameOver) {
        activeSynergies_ = computeSynergiesFromBoard();
        recomputeAllPlayerEffectiveStats();
    }
}

std::vector<SynergyStatus> GameState::computeSynergiesFromBoard() const {
    std::unordered_map<std::string, std::set<std::string>> contributors;
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
                contributors[trait].insert(boardUnit->definitionId());
            }
        }
    }

    auto makeStatus = [&](std::string trait,
                          std::array<int, 2> thresholds,
                          std::string lowEffect,
                          std::string highEffect) {
        const int count = static_cast<int>(contributors[trait].size());
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
    result.push_back(makeStatus("Warrior", {2, 4}, "Warrior ATK +10", "Warrior ATK +25"));
    result.push_back(makeStatus("Guardian", {2, 4}, "Guardian MaxHP +100", "Guardian MaxHP +250"));
    result.push_back(makeStatus("Mystic", {2, 3}, "Mystic MaxMana -10", "Mystic MaxMana -20"));
    result.push_back(makeStatus("Ranger", {2, 2}, "Ranger attack interval -10%", "Ranger attack interval -10%"));
    result.push_back(makeStatus("Healer", {2, 2}, "Healing +20%", "Healing +20%"));
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
    const std::size_t index = static_cast<std::size_t>((player_.currentRound() - 1) % static_cast<int>(definitions.size()));
    addItemToInventory(definitions[index].itemDefId);
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
    runtime_.clear();
    currentEnemyUnits_.clear();
    if (phase_ != GamePhase::Resolve) {
        playerCombatSnapshot_.clear();
    }
}

std::vector<UnitId> GameState::activeUnitsForOwner(Owner owner) const {
    std::vector<UnitId> result;
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
