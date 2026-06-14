#include "GameState.h"

#include "EncounterGenerator.h"

#include <stdexcept>

namespace synera {

GameState::GameState(int rows, int cols, std::size_t benchCapacity)
    : board_(rows, cols), bench_(benchCapacity) {}

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

Unit* GameState::unit(UnitId id) {
    return units_.get(id);
}

const Unit* GameState::unit(UnitId id) const {
    return units_.get(id);
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
    added->setPlacement(Placement::bench(*slot));
    bench_.setOccupant(*slot, id);
    return id;
}

bool GameState::deployFromBench(std::size_t slot, Position target, PlacementPolicy policy) {
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
        bench_.clear(slot);
        board_.setOccupant(target, *movingId);
        moving->setPlacement(Placement::board(target));
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
    return true;
}

bool GameState::moveBoardUnit(Position from, Position to, PlacementPolicy policy) {
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
    return true;
}

bool GameState::returnToBench(Position from, std::size_t slot) {
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
    return true;
}

std::vector<UnitId> GameState::generateEnemiesForRound(int round) {
    clearGeneratedEnemies();
    player_.setCurrentRound(round);
    generatedEnemyUnits_ = EncounterGenerator::generate(*this, round);
    return generatedEnemyUnits_;
}

std::vector<UnitId> GameState::activePlayerUnits() const {
    return activeUnitsForOwner(Owner::PlayerCtrl);
}

std::vector<UnitId> GameState::activeEnemyUnits() const {
    return activeUnitsForOwner(Owner::EnemyCtrl);
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
    for (UnitId id : generatedEnemyUnits_) {
        Unit* enemy = units_.get(id);
        if (enemy != nullptr && enemy->placement().kind == PlacementKind::BoardCell) {
            board_.clear(*enemy->placement().boardCell);
        }
        units_.remove(id);
    }
    generatedEnemyUnits_.clear();
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
    return result;
}

}  // namespace synera
