#include "Board.h"

#include <stdexcept>

namespace synera {

Board::Board(int rows, int cols) : rows_(rows), cols_(cols) {
    if (rows_ <= 0 || cols_ <= 0) {
        throw std::invalid_argument("Board dimensions must be positive.");
    }
    cells_.resize(static_cast<std::size_t>(rows_ * cols_));
}

int Board::rows() const {
    return rows_;
}

int Board::cols() const {
    return cols_;
}

bool Board::isInside(Position position) const {
    return position.row >= 0 && position.row < rows_ && position.col >= 0 && position.col < cols_;
}

bool Board::isPlayerHalf(Position position) const {
    return isInside(position) && position.row >= rows_ / 2;
}

bool Board::isEnemyHalf(Position position) const {
    return isInside(position) && position.row < rows_ / 2;
}

bool Board::isEmpty(Position position) const {
    return isInside(position) && !cells_[index(position)].has_value();
}

bool Board::canPlace(const Unit& unit, Position position) const {
    if (!isInside(position) || !isEmpty(position)) {
        return false;
    }
    if (unit.owner() == Owner::PlayerCtrl) {
        return isPlayerHalf(position);
    }
    return isEnemyHalf(position);
}

std::optional<UnitId> Board::occupant(Position position) const {
    if (!isInside(position)) {
        return std::nullopt;
    }
    return cells_[index(position)];
}

std::size_t Board::index(Position position) const {
    return static_cast<std::size_t>(position.row * cols_ + position.col);
}

void Board::setOccupant(Position position, UnitId id) {
    cells_[index(position)] = id;
}

void Board::clear(Position position) {
    cells_[index(position)].reset();
}

}  // namespace synera
