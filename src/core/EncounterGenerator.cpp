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

EnemyTemplate zombieTemplate() {
    return EnemyTemplate{"普通僵尸",
                         UnitStats{185, 23, 1, 60, 60, 20},
                         {"zombie"},
                         "enemies/zombie",
                         "BasicUnit"};
}

EnemyTemplate coneheadTemplate() {
    return EnemyTemplate{"路障僵尸",
                         UnitStats{250, 26, 1, 60, 62, 20},
                         {"zombie", "armored"},
                         "enemies/conehead_zombie",
                         "BasicUnit"};
}

EnemyTemplate screenDoorTemplate() {
    return EnemyTemplate{"铁门僵尸",
                         UnitStats{330, 24, 1, 60, 68, 20},
                         {"zombie", "armored"},
                         "enemies/screendoor_zombie",
                         "BasicUnit"};
}

EnemyTemplate newspaperTemplate() {
    return EnemyTemplate{"读报僵尸",
                         UnitStats{260, 36, 1, 60, 50, 20},
                         {"zombie", "frenzy"},
                         "enemies/newspaper_zombie",
                         "BasicUnit"};
}

EnemyTemplate footballTemplate() {
    return EnemyTemplate{"橄榄球僵尸",
                         UnitStats{430, 42, 1, 60, 44, 20},
                         {"zombie", "elite"},
                         "enemies/football_zombie",
                         "BasicUnit"};
}

EnemyTemplate jackboxTemplate() {
    return EnemyTemplate{"小丑僵尸",
                         UnitStats{330, 55, 1, 50, 54, 20},
                         {"zombie", "elite"},
                         "enemies/jackbox_zombie",
                         "BasicUnit"};
}

std::unique_ptr<Unit> makeEnemy(const EnemyTemplate& enemyTemplate, int round) {
    const int scalingRounds = std::max(0, round - 1);
    UnitStats stats = enemyTemplate.stats;
    stats.maxHp += scalingRounds * 25;
    stats.atk += scalingRounds * 4;

    return std::make_unique<BasicUnit>(enemyTemplate.name,
                                       Owner::EnemyCtrl,
                                       stats.maxHp,
                                       stats.atk,
                                       stats.range,
                                       stats.maxMana,
                                       enemyTemplate.traits,
                                       enemyTemplate.visualKey);
}

}  // namespace

std::vector<EnemyTemplate> EncounterGenerator::templatesForRound(int round) {
    const int clampedRound = std::max(1, round);
    if (clampedRound == 1) {
        return {zombieTemplate()};
    }
    if (clampedRound == 2) {
        return {zombieTemplate(), coneheadTemplate()};
    }
    if (clampedRound == 3) {
        return {zombieTemplate(), coneheadTemplate(), screenDoorTemplate()};
    }
    if (clampedRound == 4) {
        return {newspaperTemplate(), coneheadTemplate(), screenDoorTemplate()};
    }
    return {footballTemplate(), jackboxTemplate(), screenDoorTemplate()};
}

std::vector<EnemySpawnPlan> EncounterGenerator::plan(const GameState& state,
                                                     int round,
                                                     bool ignoreExistingEnemies) {
    const int clampedRound = std::max(1, round);
    const std::vector<EnemyTemplate> templates = templatesForRound(clampedRound);
    const int desiredCount = static_cast<int>(templates.size());
    const std::vector<Position> cells = deterministicEnemyCells(state, ignoreExistingEnemies);
    const int spawnCount = std::min(desiredCount, static_cast<int>(cells.size()));

    std::vector<EnemySpawnPlan> result;
    result.reserve(static_cast<std::size_t>(spawnCount));
    for (int i = 0; i < spawnCount; ++i) {
        result.push_back(EnemySpawnPlan{cells[static_cast<std::size_t>(i)],
                                        makeEnemy(templates[static_cast<std::size_t>(i)], clampedRound)});
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
