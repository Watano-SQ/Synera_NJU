#pragma once

#include "Types.h"
#include "Unit.h"

#include <memory>
#include <vector>

namespace synera {

class GameState;

struct EnemySpawnPlan {
    Position position;
    std::unique_ptr<Unit> unit;
};

class EncounterGenerator {
public:
    static std::vector<EnemySpawnPlan> plan(const GameState& state, int round, bool ignoreExistingEnemies = false);
    static std::vector<UnitId> generate(GameState& state, int round);
};

}  // namespace synera
