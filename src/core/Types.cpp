#include "Types.h"

#include <sstream>

namespace synera {

std::string toString(Owner owner) {
    switch (owner) {
        case Owner::PlayerCtrl:
            return "PlayerCtrl";
        case Owner::EnemyCtrl:
            return "EnemyCtrl";
    }
    return "UnknownOwner";
}

std::string toString(UnitState state) {
    switch (state) {
        case UnitState::Idle:
            return "Idle";
        case UnitState::Moving:
            return "Moving";
        case UnitState::Attacking:
            return "Attacking";
        case UnitState::Casting:
            return "Casting";
        case UnitState::Dead:
            return "Dead";
    }
    return "UnknownState";
}

std::string toString(GamePhase phase) {
    switch (phase) {
        case GamePhase::Prep:
            return "Prep";
        case GamePhase::Combat:
            return "Combat";
        case GamePhase::Resolve:
            return "Resolve";
        case GamePhase::GameOver:
            return "GameOver";
    }
    return "UnknownPhase";
}

std::string toString(RoundResult result) {
    switch (result) {
        case RoundResult::None:
            return "None";
        case RoundResult::PlayerVictory:
            return "PlayerVictory";
        case RoundResult::PlayerDefeat:
            return "PlayerDefeat";
    }
    return "UnknownRoundResult";
}

std::string toString(MatchResult result) {
    switch (result) {
        case MatchResult::Ongoing:
            return "Ongoing";
        case MatchResult::PlayerVictory:
            return "PlayerVictory";
        case MatchResult::PlayerDefeat:
            return "PlayerDefeat";
    }
    return "UnknownMatchResult";
}

std::string toString(ItemEffectType effectType) {
    switch (effectType) {
        case ItemEffectType::Attack:
            return "Attack";
        case ItemEffectType::MaxHp:
            return "MaxHp";
        case ItemEffectType::MaxMana:
            return "MaxMana";
        case ItemEffectType::AttackIntervalPercent:
            return "AttackIntervalPercent";
    }
    return "UnknownItemEffect";
}

std::string toString(const Position& position) {
    std::ostringstream out;
    out << "(" << position.row << ", " << position.col << ")";
    return out.str();
}

std::string toString(const Placement& placement) {
    std::ostringstream out;
    switch (placement.kind) {
        case PlacementKind::None:
            return "None";
        case PlacementKind::BenchSlot:
            out << "Bench[" << *placement.benchSlot << "]";
            return out.str();
        case PlacementKind::BoardCell:
            out << "Board" << toString(*placement.boardCell);
            return out.str();
    }
    return "UnknownPlacement";
}

std::string toString(const UnitStats& stats) {
    std::ostringstream out;
    out << "HP " << stats.maxHp << " ATK " << stats.atk << " Range " << stats.range << " Mana "
        << stats.maxMana << " AtkInt " << stats.attackInterval << " MoveInt " << stats.moveInterval;
    return out.str();
}

}  // namespace synera
