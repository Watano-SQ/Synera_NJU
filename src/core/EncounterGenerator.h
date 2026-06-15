#pragma once

#include "Types.h"
#include "Unit.h"

#include <memory>
#include <string>
#include <vector>

namespace synera {

class GameState;

struct EnemySpawnPlan {
    Position position;
    std::unique_ptr<Unit> unit;
};

struct EnemyTemplate {
    std::string name;
    UnitStats stats;
    std::vector<std::string> traits;
    std::string visualKey;
    std::string factoryKey;
};

class EncounterGenerator {
public:
    static std::vector<EnemyTemplate> templatesForRound(int round);
    static std::vector<EnemySpawnPlan> plan(const GameState& state, int round, bool ignoreExistingEnemies = false);
    static std::vector<UnitId> generate(GameState& state, int round);
};

}  // namespace synera
