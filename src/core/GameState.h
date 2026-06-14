#pragma once

#include "Bench.h"
#include "Board.h"
#include "Player.h"
#include "Types.h"
#include "UnitManager.h"

#include <memory>
#include <vector>

namespace synera {

class EncounterGenerator;

class GameState {
public:
    GameState(int rows = 8, int cols = 8, std::size_t benchCapacity = 8);

    Player& player();
    const Player& player() const;
    Board& board();
    const Board& board() const;
    Bench& bench();
    const Bench& bench() const;
    UnitManager& units();
    const UnitManager& units() const;

    Unit* unit(UnitId id);
    const Unit* unit(UnitId id) const;

    UnitId addUnitToBench(std::unique_ptr<Unit> unit);
    bool deployFromBench(std::size_t slot, Position target, PlacementPolicy policy = PlacementPolicy::Reject);
    bool moveBoardUnit(Position from, Position to, PlacementPolicy policy = PlacementPolicy::Reject);
    bool returnToBench(Position from, std::size_t slot);
    std::vector<UnitId> generateEnemiesForRound(int round);

    std::vector<UnitId> activePlayerUnits() const;
    std::vector<UnitId> activeEnemyUnits() const;

private:
    friend class EncounterGenerator;

    bool canOccupyHalf(const Unit& unit, Position position) const;
    UnitId addEnemyToBoard(std::unique_ptr<Unit> unit, Position position);
    void clearGeneratedEnemies();
    std::vector<UnitId> activeUnitsForOwner(Owner owner) const;

    Player player_;
    Board board_;
    Bench bench_;
    UnitManager units_;
    std::vector<UnitId> generatedEnemyUnits_;
};

}  // namespace synera
