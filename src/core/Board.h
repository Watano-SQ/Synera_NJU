#pragma once

#include "Types.h"
#include "Unit.h"

#include <optional>
#include <vector>

namespace synera {

class GameState;

class Board {
public:
    Board(int rows = 8, int cols = 8);

    int rows() const;
    int cols() const;
    bool isInside(Position position) const;
    bool isPlayerHalf(Position position) const;
    bool isEnemyHalf(Position position) const;
    bool isEmpty(Position position) const;
    bool canPlace(const Unit& unit, Position position) const;
    std::optional<UnitId> occupant(Position position) const;

private:
    friend class GameState;

    std::size_t index(Position position) const;
    void setOccupant(Position position, UnitId id);
    void clear(Position position);

    int rows_;
    int cols_;
    std::vector<std::optional<UnitId>> cells_;
};

}  // namespace synera
