#include "core/GameState.h"
#include "core/Unit.h"

#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace synera;

namespace {

std::unique_ptr<Unit> playerUnit(const std::string& name = "Player Unit") {
    return std::make_unique<BasicUnit>(name, Owner::PlayerCtrl, 300, 30, 1, 60,
                                       std::vector<std::string>{"Warrior", "Human"},
                                       "player_basic");
}

std::unique_ptr<Unit> enemyUnit(const std::string& name = "Enemy Unit") {
    return std::make_unique<BasicUnit>(name, Owner::EnemyCtrl, 200, 22, 1, 60,
                                       std::vector<std::string>{"Neutral"}, "enemy_basic");
}

void testBoardAndHalfRules() {
    GameState game(8, 8, 8);
    UnitId id = game.addUnitToBench(playerUnit());
    Unit* unit = game.unit(id);

    assert(game.board().rows() == 8);
    assert(game.board().cols() == 8);
    assert(game.board().isInside(Position{0, 0}));
    assert(!game.board().isInside(Position{-1, 0}));
    assert(!game.board().isInside(Position{8, 0}));
    assert(game.board().isEnemyHalf(Position{0, 0}));
    assert(game.board().isPlayerHalf(Position{4, 0}));
    assert(!game.board().canPlace(*unit, Position{0, 0}));
    assert(game.board().canPlace(*unit, Position{7, 7}));
}

void testBenchCapacityAndSync() {
    GameState game(8, 8, 2);
    UnitId first = game.addUnitToBench(playerUnit("First"));
    UnitId second = game.addUnitToBench(playerUnit("Second"));

    assert(game.bench().occupant(0) == first);
    assert(game.bench().occupant(-1) == std::nullopt);
    assert(game.bench().occupant(99) == std::nullopt);
    assert(game.bench().occupant(1) == second);
    assert(!game.bench().firstEmptySlot().has_value());

    bool threw = false;
    try {
        game.addUnitToBench(playerUnit("Overflow"));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    assert(game.deployFromBench(0, Position{7, 0}));
    assert(!game.bench().occupant(0).has_value());
    assert(game.board().occupant(Position{7, 0}) == first);
    const Position firstBoardPosition{7, 0};
    assert(game.unit(first)->placement().kind == PlacementKind::BoardCell);
    assert(*game.unit(first)->placement().boardCell == firstBoardPosition);

    assert(game.returnToBench(Position{7, 0}, 0));
    assert(game.bench().occupant(0) == first);
    assert(!game.board().occupant(Position{7, 0}).has_value());
    assert(game.unit(first)->placement().kind == PlacementKind::BenchSlot);
    assert(*game.unit(first)->placement().benchSlot == 0);
}

void testRejectAndSwapPlacement() {
    GameState game;
    UnitId first = game.addUnitToBench(playerUnit("First"));
    UnitId second = game.addUnitToBench(playerUnit("Second"));

    assert(game.deployFromBench(0, Position{7, 0}));
    assert(!game.deployFromBench(1, Position{0, 0}));
    assert(game.bench().occupant(1) == second);
    assert(!game.board().occupant(Position{0, 0}).has_value());

    assert(!game.deployFromBench(1, Position{7, 0}, PlacementPolicy::Reject));
    assert(game.bench().occupant(1) == second);
    assert(game.board().occupant(Position{7, 0}) == first);

    assert(game.deployFromBench(1, Position{7, 0}, PlacementPolicy::Swap));
    assert(game.bench().occupant(1) == first);
    assert(game.board().occupant(Position{7, 0}) == second);
    assert(*game.unit(first)->placement().benchSlot == 1);
    const Position secondBoardPosition{7, 0};
    assert(*game.unit(second)->placement().boardCell == secondBoardPosition);

    assert(game.moveBoardUnit(Position{7, 0}, Position{6, 0}));
    assert(!game.board().occupant(Position{7, 0}).has_value());
    assert(game.board().occupant(Position{6, 0}) == second);
}

void testEnemyTypeAndRoundGeneration() {
    GameState game;
    UnitId player = game.addUnitToBench(playerUnit());
    assert(game.deployFromBench(0, Position{7, 2}));

    std::vector<UnitId> enemies = game.generateEnemiesForRound(1);
    assert(!enemies.empty());
    assert(game.player().currentRound() == 1);

    for (UnitId id : enemies) {
        const Unit* enemy = game.unit(id);
        assert(enemy != nullptr);
        assert(enemy->owner() == Owner::EnemyCtrl);
        assert(enemy->placement().kind == PlacementKind::BoardCell);
        assert(game.board().isEnemyHalf(*enemy->placement().boardCell));
        assert(!enemy->traits().empty());
    }

    std::vector<UnitId> ar = game.activePlayerUnits();
    std::vector<UnitId> er = game.activeEnemyUnits();
    assert(ar.size() == 1);
    assert(ar[0] == player);
    assert(er.size() == enemies.size());

    const Unit* playerUnitPtr = game.unit(player);
    assert(playerUnitPtr->owner() == Owner::PlayerCtrl);
    assert(playerUnitPtr->traits().size() == 2);
}

void testEnemyCannotUsePlayerBench() {
    GameState game;
    bool threw = false;
    try {
        game.addUnitToBench(enemyUnit());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    std::vector<UnitId> enemies = game.generateEnemiesForRound(1);
    const Unit* enemy = game.unit(enemies.front());
    assert(enemy != nullptr);
    assert(!game.returnToBench(*enemy->placement().boardCell, 0));
    assert(game.board().occupant(*enemy->placement().boardCell) == enemies.front());
}

}  // namespace

int main() {
    testBoardAndHalfRules();
    testBenchCapacityAndSync();
    testRejectAndSwapPlacement();
    testEnemyTypeAndRoundGeneration();
    testEnemyCannotUsePlayerBench();
    return 0;
}
