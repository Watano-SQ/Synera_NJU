#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace synera {

using UnitId = std::uint64_t;
using ItemId = std::uint64_t;

enum class Owner {
    PlayerCtrl,
    EnemyCtrl
};

enum class UnitState {
    Idle,
    Moving,
    Attacking,
    Casting,
    Dead
};

enum class GamePhase {
    Prep,
    Combat,
    Resolve,
    GameOver
};

enum class RoundResult {
    None,
    PlayerVictory,
    PlayerDefeat
};

enum class MatchResult {
    Ongoing,
    PlayerVictory,
    PlayerDefeat
};

enum class PlacementKind {
    None,
    BenchSlot,
    BoardCell
};

enum class PlacementPolicy {
    Reject,
    Swap
};

enum class ItemEffectType {
    Attack,
    MaxHp,
    MaxMana,
    AttackIntervalPercent
};

struct Position {
    int row = 0;
    int col = 0;

    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }

    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
};

struct ActionResult {
    bool success = false;
    std::string message;
};

struct UnitStats {
    int maxHp = 1;
    int atk = 1;
    int range = 1;
    int maxMana = 60;
    int attackInterval = 60;
    int moveInterval = 20;
};

struct UnitDefinition {
    std::string definitionId;
    std::string name;
    int cost = 1;
    std::vector<std::string> traits;
    UnitStats baseStats;
    std::string factoryKey;
    std::string visualKey;
};

struct ShopOffer {
    std::string definitionId;
    int cost = 0;
};

struct ItemDefinition {
    std::string itemDefId;
    std::string name;
    ItemEffectType effectType = ItemEffectType::Attack;
    int value = 0;
};

struct ItemInstance {
    ItemId itemId = 0;
    std::string itemDefId;
};

struct SynergyStatus {
    std::string trait;
    int count = 0;
    int activeThreshold = 0;
    int nextThreshold = 0;
    bool active = false;
    std::string effectDescription;
};

struct CombatConfig {
    int attackInterval = 60;
    int moveInterval = 20;
    int manaPerAttack = 10;
    int victoryGold = 5;
    int defeatGold = 2;
    int defeatHpLoss = 10;
    int maxRound = 3;
    int itemDropPercent = 35;
};

struct Placement {
    PlacementKind kind = PlacementKind::None;
    std::optional<std::size_t> benchSlot;
    std::optional<Position> boardCell;

    static Placement none() {
        return {};
    }

    static Placement bench(std::size_t slot) {
        Placement placement;
        placement.kind = PlacementKind::BenchSlot;
        placement.benchSlot = slot;
        return placement;
    }

    static Placement board(Position position) {
        Placement placement;
        placement.kind = PlacementKind::BoardCell;
        placement.boardCell = position;
        return placement;
    }
};

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
