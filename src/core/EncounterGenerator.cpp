#include "EncounterGenerator.h"

#include "GameState.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace synera {

namespace {

bool isSpawnCellAvailable(const GameState& state, Position position, bool ignoreExistingEnemies) {
    if (!state.board().isEnemyHalf(position)) {
        return false;
    }

    const auto occupant = state.board().occupant(position);
    if (!occupant.has_value()) {
        return true;
    }

    const Unit* unit = state.unit(*occupant);
    return ignoreExistingEnemies && unit != nullptr && unit->owner() == Owner::EnemyCtrl;
}

std::vector<Position> deterministicEnemyCells(const GameState& state, bool ignoreExistingEnemies) {
    std::vector<Position> cells;
    const int enemyRows = state.board().rows() / 2;
    const int center = state.board().cols() / 2;

    for (int row = 0; row < enemyRows; ++row) {
        std::vector<int> cols;
        cols.reserve(static_cast<std::size_t>(state.board().cols()));
        for (int offset = 0; offset < state.board().cols(); ++offset) {
            const int left = center - offset;
            const int right = center + offset;
            if (left >= 0) {
                cols.push_back(left);
            }
            if (right >= 0 && right < state.board().cols() && right != left) {
                cols.push_back(right);
            }
        }

        for (int col : cols) {
            const Position position{row, col};
            if (isSpawnCellAvailable(state, position, ignoreExistingEnemies)) {
                cells.push_back(position);
            }
        }
    }

    return cells;
}

std::unique_ptr<Unit> makeEnemy(int round, int index) {
    const int clampedRound = std::max(1, round);
    const int hp = 150 + clampedRound * 35 + index * 25;
    const int atk = 18 + clampedRound * 5 + index * 3;
    std::vector<std::string> traits{"Neutral"};
    if (clampedRound >= 3 || index >= 2) {
        traits.push_back("Elite");
    }

    return std::make_unique<BasicUnit>(
        "Round " + std::to_string(clampedRound) + " Enemy " + std::to_string(index + 1),
        Owner::EnemyCtrl,
        hp,
        atk,
        1,
        60,
        traits,
        index >= 2 ? "enemy_elite" : "enemy_basic");
}

}  // namespace

std::vector<EnemySpawnPlan> EncounterGenerator::plan(const GameState& state,
                                                     int round,
                                                     bool ignoreExistingEnemies) {
    const int clampedRound = std::max(1, round);
    const int desiredCount = std::min(3, clampedRound);
    const std::vector<Position> cells = deterministicEnemyCells(state, ignoreExistingEnemies);
    const int spawnCount = std::min(desiredCount, static_cast<int>(cells.size()));

    std::vector<EnemySpawnPlan> result;
    result.reserve(static_cast<std::size_t>(spawnCount));
    for (int i = 0; i < spawnCount; ++i) {
        result.push_back(EnemySpawnPlan{cells[static_cast<std::size_t>(i)], makeEnemy(clampedRound, i)});
    }
    return result;
}

std::vector<UnitId> EncounterGenerator::generate(GameState& state, int round) {
    auto spawnPlan = plan(state, round);
    std::vector<UnitId> enemies;
    enemies.reserve(spawnPlan.size());
    for (auto& spawn : spawnPlan) {
        enemies.push_back(state.addEnemyToBoard(std::move(spawn.unit), spawn.position));
    }
    return enemies;
}

}  // namespace synera
