#include "core/GameState.h"
#include "core/Unit.h"

#include <iostream>
#include <memory>
#include <string>

using namespace synera;

namespace {

std::unique_ptr<Unit> makePlayerUnit(const std::string& name, std::vector<std::string> traits) {
    return std::make_unique<BasicUnit>(name, Owner::PlayerCtrl, 320, 35, 1, 60, std::move(traits),
                                       "units/peashooter");
}

void printUnit(const GameState& game, UnitId id) {
    const Unit* unit = game.unit(id);
    if (unit == nullptr) {
        return;
    }

    std::cout << "#" << id << " " << unit->name() << " owner=" << toString(unit->owner())
              << " hp=" << unit->hp() << "/" << unit->maxHp() << " atk=" << unit->atk()
              << " range=" << unit->range() << " mana=" << unit->mana() << "/"
              << unit->maxMana() << " at " << toString(unit->placement()) << "\n";
}

void printBench(const GameState& game) {
    std::cout << "\nBench:\n";
    for (std::size_t slot = 0; slot < game.bench().capacity(); ++slot) {
        std::cout << "  [" << slot << "] ";
        const auto occupant = game.bench().occupant(slot);
        if (occupant.has_value()) {
            printUnit(game, *occupant);
        } else {
            std::cout << "empty\n";
        }
    }
}

void printBoard(const GameState& game) {
    std::cout << "\nBoard occupancy:\n";
    for (int row = 0; row < game.board().rows(); ++row) {
        std::cout << "  row " << row << ": ";
        for (int col = 0; col < game.board().cols(); ++col) {
            const auto occupant = game.board().occupant(Position{row, col});
            if (!occupant.has_value()) {
                std::cout << ". ";
                continue;
            }

            const Unit* unit = game.unit(*occupant);
            std::cout << (unit->owner() == Owner::PlayerCtrl ? "P" : "E") << *occupant << " ";
        }
        std::cout << "\n";
    }
}

void printActiveSet(const GameState& game, const std::string& label, const std::vector<UnitId>& ids) {
    std::cout << "\n" << label << ":\n";
    for (UnitId id : ids) {
        std::cout << "  ";
        printUnit(game, id);
    }
}

}  // namespace

int main() {
    GameState game;

    const UnitId peashooter = game.addUnitToBench(makePlayerUnit("豌豆射手", {"shooter"}));
    const UnitId sunflower = game.addUnitToBench(makePlayerUnit("向日葵", {"sun", "healer"}));

    std::cout << "Created player units: #" << peashooter << ", #" << sunflower << "\n";

    const bool placedPeashooter = game.deployFromBench(0, Position{7, 3});
    const bool illegalSunflowerPlacement = game.deployFromBench(1, Position{0, 0});
    const bool placedSunflower = game.deployFromBench(1, Position{6, 4});

    std::cout << "Deploy peashooter to player half: " << (placedPeashooter ? "ok" : "failed") << "\n";
    std::cout << "Deploy sunflower to enemy half: " << (illegalSunflowerPlacement ? "ok" : "rejected") << "\n";
    std::cout << "Deploy sunflower to player half: " << (placedSunflower ? "ok" : "failed") << "\n";

    const std::vector<UnitId> enemies = game.generateEnemiesForRound(1);
    std::cout << "Generated " << enemies.size() << " enemies for wave "
              << game.player().currentRound() << ".\n";

    printBench(game);
    printBoard(game);
    printActiveSet(game, "Player combat units", game.activePlayerUnits());
    printActiveSet(game, "Enemy combat units", game.activeEnemyUnits());

    return 0;
}
