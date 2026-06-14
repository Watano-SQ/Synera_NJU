#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace synera {

using UnitId = std::uint64_t;

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

enum class PlacementKind {
    None,
    BenchSlot,
    BoardCell
};

enum class PlacementPolicy {
    Reject,
    Swap
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
std::string toString(const Position& position);
std::string toString(const Placement& placement);

}  // namespace synera
