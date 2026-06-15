#include "core/GameState.h"
#include "core/Unit.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <vector>

using namespace synera;

namespace {

CombatConfig fastConfig() {
    CombatConfig config;
    config.attackInterval = 1;
    config.moveInterval = 1;
    config.manaPerAttack = 60;
    config.victoryGold = 5;
    config.defeatGold = 2;
    config.defeatHpLoss = 10;
    config.maxRound = 3;
    return config;
}

std::unique_ptr<Unit> basicPlayer(const std::string& name = "Player",
                                  int hp = 300,
                                  int atk = 40,
                                  int range = 1,
                                  int maxMana = 60) {
    return std::make_unique<BasicUnit>(name, Owner::PlayerCtrl, hp, atk, range, maxMana,
                                       std::vector<std::string>{"Human"}, "player_basic");
}

std::unique_ptr<Unit> vanguardPlayer(int atk = 40, int range = 1) {
    return std::make_unique<Vanguard>("Aster Vanguard", Owner::PlayerCtrl, 500, atk, range, 60,
                                      std::vector<std::string>{"Warrior"}, "player_vanguard");
}

std::unique_ptr<Unit> sparkPlayer(int atk = 40, int range = 2) {
    return std::make_unique<SparkMage>("Mira Spark", Owner::PlayerCtrl, 500, atk, range, 60,
                                       std::vector<std::string>{"Mage"}, "player_mage");
}

void deployOne(GameState& game, Position position, std::unique_ptr<Unit> unit = basicPlayer()) {
    game.addUnitToBench(std::move(unit));
    assert(game.deployFromBench(0, position));
}

void testPhaseGuardsAndCombatDeploymentRejection() {
    GameState game(8, 8, 8, fastConfig());
    deployOne(game, Position{7, 4});

    assert(!game.tickCombat().success);
    assert(!game.resolveRound().success);
    assert(game.phase() == GamePhase::Prep);

    const ActionResult start = game.startCombat();
    assert(start.success);
    assert(game.phase() == GamePhase::Combat);
    assert(!game.startCombat().success);
    assert(!game.deployFromBench(0, Position{7, 3}));
    assert(!game.moveBoardUnit(Position{7, 4}, Position{7, 3}));
    assert(!game.returnToBench(Position{7, 4}, 1));
}

void testNoActivePlayerUnitsLosesRoundWithoutAdvancing() {
    GameState game(8, 8, 8, fastConfig());
    game.player().setCurrentRound(3);
    game.addUnitToBench(basicPlayer("Bench Only"));

    const ActionResult start = game.startCombat();
    assert(start.success);
    assert(game.phase() == GamePhase::Resolve);
    assert(game.lastRoundResult() == RoundResult::PlayerDefeat);
    assert(game.activeCombatUnits().empty());

    const ActionResult resolve = game.resolveRound();
    assert(resolve.success);
    assert(game.phase() == GamePhase::Prep);
    assert(game.matchResult() == MatchResult::Ongoing);
    assert(game.player().currentRound() == 3);
    assert(game.player().hp() == 90);
}

void testCurrentEnemyUnitsIncludeDeadUntilResolve() {
    GameState game(8, 8, 8, fastConfig());
    UnitId player = game.addUnitToBench(basicPlayer("Sniper", 500, 999, 10));
    assert(game.deployFromBench(0, Position{7, 4}));

    assert(game.startCombat().success);
    assert(game.currentEnemyUnits().size() == 1);
    const UnitId enemy = game.currentEnemyUnits().front();
    const Position enemyPosition = *game.unit(enemy)->placement().boardCell;

    assert(game.tickCombat().success);
    assert(game.phase() == GamePhase::Resolve);
    assert(game.currentEnemyUnits().size() == 1);
    assert(game.currentEnemyUnits().front() == enemy);
    assert(game.unit(enemy) != nullptr);
    assert(!game.unit(enemy)->isAlive());
    assert(!game.board().occupant(enemyPosition).has_value());
    assert(game.activeEnemyUnits().empty());
    assert(game.unit(player)->isAlive());

    assert(game.resolveRound().success);
    assert(game.currentEnemyUnits().empty());
    assert(game.unit(enemy) == nullptr);
}

void testBenchUnitsDoNotJoinActiveCombatList() {
    GameState game(8, 8, 8, fastConfig());
    const UnitId deployed = game.addUnitToBench(basicPlayer("Deployed", 400, 40, 10));
    const UnitId benched = game.addUnitToBench(basicPlayer("Benched", 400, 40, 10));
    assert(game.deployFromBench(0, Position{7, 4}));

    assert(game.startCombat().success);
    const std::vector<UnitId> active = game.activeCombatUnits();
    assert(std::find(active.begin(), active.end(), deployed) != active.end());
    assert(std::find(active.begin(), active.end(), benched) == active.end());
}

void testSpawnFailureIsAtomic() {
    GameState game(1, 1, 2, fastConfig());
    deployOne(game, Position{0, 0});
    const UnitId player = *game.board().occupant(Position{0, 0});

    const ActionResult start = game.startCombat();
    assert(!start.success);
    assert(game.phase() == GamePhase::Prep);
    assert(game.currentEnemyUnits().empty());
    assert(game.board().occupant(Position{0, 0}) == player);
    assert(game.unit(player)->placement().kind == PlacementKind::BoardCell);
}

void testBasicAttackManaDoesNotCastUntilNextReadyAttack() {
    GameState game(8, 8, 8, fastConfig());
    UnitId player = game.addUnitToBench(vanguardPlayer(1, 10));
    assert(game.deployFromBench(0, Position{7, 4}));
    assert(game.startCombat().success);
    const UnitId enemy = game.currentEnemyUnits().front();
    const int initialEnemyHp = game.unit(enemy)->hp();

    assert(game.tickCombat().success);
    assert(game.unit(player)->mana() == game.unit(player)->maxMana());
    assert(game.unit(enemy)->hp() == initialEnemyHp - 1);

    assert(game.tickCombat().success);
    assert(game.unit(player)->mana() == 0);
    assert(game.unit(enemy)->hp() == initialEnemyHp - 3);
}

void testTargetTieBreakPrefersLowerColumnWhenHpTied() {
    GameState game(8, 8, 8, fastConfig());
    game.player().setCurrentRound(3);
    game.addUnitToBench(basicPlayer("Tie Breaker", 500, 1, 10));
    assert(game.deployFromBench(0, Position{7, 4}));
    assert(game.startCombat().success);

    UnitId centerEnemy = 0;
    UnitId leftEnemy = 0;
    UnitId rightEnemy = 0;
    for (UnitId id : game.currentEnemyUnits()) {
        const Position position = *game.unit(id)->placement().boardCell;
        if (position.col == 4) {
            centerEnemy = id;
        } else if (position.col == 3) {
            leftEnemy = id;
        } else if (position.col == 5) {
            rightEnemy = id;
        }
    }
    assert(centerEnemy != 0 && leftEnemy != 0 && rightEnemy != 0);

    game.unit(centerEnemy)->setHp(0);
    game.unit(rightEnemy)->setHp(game.unit(leftEnemy)->hp());
    const int leftHp = game.unit(leftEnemy)->hp();
    const int rightHp = game.unit(rightEnemy)->hp();

    assert(game.tickCombat().success);
    assert(game.unit(leftEnemy)->hp() == leftHp - 1);
    assert(game.unit(rightEnemy)->hp() == rightHp);
}

void testMoveConflictLeavesContestedCellEmpty() {
    GameState game(4, 3, 6, fastConfig());
    game.player().setUnitCap(4);
    const UnitId leftAttacker = game.addUnitToBench(basicPlayer("Left", 300, 10, 1));
    assert(game.deployFromBench(0, Position{3, 0}));
    const UnitId rightAttacker = game.addUnitToBench(basicPlayer("Right", 300, 10, 1));
    assert(game.deployFromBench(0, Position{3, 2}));
    game.addUnitToBench(basicPlayer("Left Blocker", 300, 10, 1));
    assert(game.deployFromBench(0, Position{2, 0}));
    game.addUnitToBench(basicPlayer("Right Blocker", 300, 10, 1));
    assert(game.deployFromBench(0, Position{2, 2}));

    assert(game.startCombat().success);
    assert(game.tickCombat().success);
    assert(game.board().occupant(Position{3, 0}) == leftAttacker);
    assert(game.board().occupant(Position{3, 2}) == rightAttacker);
    assert(!game.board().occupant(Position{3, 1}).has_value());
}

void testResolveRestoresPlayersAndBench() {
    GameState game(8, 8, 8, fastConfig());
    const UnitId player = game.addUnitToBench(vanguardPlayer(999, 10));
    const UnitId bench = game.addUnitToBench(basicPlayer("Reserve", 300, 40, 1));
    assert(game.deployFromBench(0, Position{7, 4}));
    const Position snapshotPosition{7, 4};

    assert(game.startCombat().success);
    Unit* playerUnit = game.unit(player);
    playerUnit->setHp(123);
    playerUnit->setMana(60);
    assert(game.tickCombat().success);
    assert(game.phase() == GamePhase::Resolve);

    assert(game.resolveRound().success);
    assert(game.phase() == GamePhase::Prep);
    assert(game.player().currentRound() == 2);
    assert(game.board().occupant(snapshotPosition) == player);
    assert(game.bench().occupant(1) == bench);
    assert(game.unit(player)->hp() == game.unit(player)->maxHp());
    assert(game.unit(player)->mana() == 0);
    assert(game.unit(player)->state() == UnitState::Idle);
    assert(*game.unit(player)->placement().boardCell == snapshotPosition);
}

void testGameOverRejectsFurtherActions() {
    GameState game(8, 8, 8, fastConfig());
    game.player().setCurrentRound(3);
    deployOne(game, Position{7, 4}, sparkPlayer(999, 10));

    assert(game.startCombat().success);
    game.unit(*game.board().occupant(Position{7, 4}))->setMana(60);
    assert(game.tickCombat().success);
    assert(game.resolveRound().success);
    assert(game.phase() == GamePhase::GameOver);
    assert(game.matchResult() == MatchResult::PlayerVictory);
    assert(!game.startCombat().success);
    assert(!game.tickCombat().success);
    assert(!game.resolveRound().success);
    assert(!game.moveBoardUnit(Position{7, 4}, Position{7, 3}));
}

}  // namespace

int main() {
    testPhaseGuardsAndCombatDeploymentRejection();
    testNoActivePlayerUnitsLosesRoundWithoutAdvancing();
    testCurrentEnemyUnitsIncludeDeadUntilResolve();
    testBenchUnitsDoNotJoinActiveCombatList();
    testSpawnFailureIsAtomic();
    testBasicAttackManaDoesNotCastUntilNextReadyAttack();
    testTargetTieBreakPrefersLowerColumnWhenHpTied();
    testMoveConflictLeavesContestedCellEmpty();
    testResolveRestoresPlayersAndBench();
    testGameOverRejectsFurtherActions();
    return 0;
}
