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

}  // namespace synera
