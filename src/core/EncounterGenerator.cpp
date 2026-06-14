#include "EncounterGenerator.h"

#include "GameState.h"
#include "Unit.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace synera {

std::vector<UnitId> EncounterGenerator::generate(GameState& state, int round) {
    const int normalizedRound = std::max(1, round);
    const int enemyHalfRows = state.board().rows() / 2;
    const int maxEnemyCells = std::max(1, enemyHalfRows * state.board().cols());
    const int enemyCount = std::min(maxEnemyCells, 1 + normalizedRound);

    std::vector<UnitId> enemies;
    enemies.reserve(static_cast<std::size_t>(enemyCount));

    for (int i = 0; i < enemyCount; ++i) {
        const int row = i / state.board().cols();
        const int col = i % state.board().cols();
        const Position spawn{row, col};
        const int hp = 120 + normalizedRound * 30 + i * 20;
        const int atk = 18 + normalizedRound * 4 + i * 2;
        const int maxMana = 60;

        std::vector<std::string> traits{"Neutral"};
        if (normalizedRound >= 3) {
            traits.push_back("Elite");
        }

        auto unit = std::make_unique<BasicUnit>(
            "Round " + std::to_string(normalizedRound) + " Enemy " + std::to_string(i + 1),
            Owner::EnemyCtrl,
            hp,
            atk,
            1,
            maxMana,
            traits,
            "enemy_basic");
        enemies.push_back(state.addEnemyToBoard(std::move(unit), spawn));
    }

    return enemies;
}

}  // namespace synera
