#include "core/Catalog.h"
#include "core/GameState.h"
#include "core/Unit.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace synera;

namespace {

CombatConfig stage3Config() {
    CombatConfig config;
    config.attackInterval = 1;
    config.moveInterval = 1;
    config.manaPerAttack = 60;
    config.itemDropPercent = 0;
    return config;
}

std::unique_ptr<Unit> unitFromDefinition(const std::string& definitionId) {
    const UnitDefinition* definition = findUnitDefinition(definitionId);
    assert(definition != nullptr);
    return createUnitFromDefinition(*definition, Owner::PlayerCtrl);
}

const SynergyStatus* findSynergy(const GameState& game, const std::string& trait) {
    for (const SynergyStatus& status : game.activeSynergies()) {
        if (status.trait == trait) {
            return &status;
        }
    }
    return nullptr;
}

void testInitialShopPurchaseAndRefresh() {
    GameState game(8, 8, 8, stage3Config());
    assert(game.shopOffers().size() == 5);
    for (const auto& offer : game.shopOffers()) {
        assert(offer.has_value());
    }

    game.player().setGold(20);
    const int beforeGold = game.player().gold();
    const int cost = game.shopOffers()[0]->cost;
    const ActionResult purchase = game.purchaseShopSlot(0);
    assert(purchase.success);
    assert(game.player().gold() == beforeGold - cost);
    assert(!game.shopOffers()[0].has_value());
    assert(game.bench().occupant(0).has_value());

    const int goldAfterPurchase = game.player().gold();
    const ActionResult refresh = game.refreshShop();
    assert(refresh.success);
    assert(game.player().gold() == goldAfterPurchase - 2);
    for (const auto& offer : game.shopOffers()) {
        assert(offer.has_value());
    }
}

void testBusinessApisArePrepOnlyAndCombatSaveRejected() {
    GameState game(8, 8, 8, stage3Config());
    game.player().setGold(20);
    game.addItemToInventory("iron_sword");
    const UnitId unitId = game.addUnitToBench(unitFromDefinition("aster_vanguard"));
    assert(game.deployFromBench(0, Position{7, 4}));
    assert(game.startCombat().success);

    assert(!game.refreshShop().success);
    assert(!game.purchaseShopSlot(0).success);
    assert(!game.upgradePopulation().success);
    assert(!game.equipItem(1, unitId).success);
    assert(!game.saveToFile("stage3_combat_should_fail.sav").success);
}

void testPopulationLimitAndUpgrade() {
    GameState game(8, 8, 8, stage3Config());
    game.player().setGold(20);
    game.addUnitToBench(unitFromDefinition("aster_vanguard"));
    game.addUnitToBench(unitFromDefinition("mira_spark"));
    game.addUnitToBench(unitFromDefinition("iris_guard"));
    game.addUnitToBench(unitFromDefinition("rowan_ranger"));

    assert(game.deployFromBench(0, Position{7, 0}));
    assert(game.deployFromBench(1, Position{7, 1}));
    assert(game.deployFromBench(2, Position{7, 2}));
    assert(!game.deployFromBench(3, Position{7, 3}));
    assert(game.deployedPlayerUnitCount() == 3);

    const ActionResult upgrade = game.upgradePopulation();
    assert(upgrade.success);
    assert(game.player().level() == 2);
    assert(game.player().unitCap() == 4);
    assert(game.deployFromBench(3, Position{7, 3}));
}

void testMergeKeepsNewestAndReturnsRemovedEquipment() {
    GameState game(8, 8, 8, stage3Config());
    const ItemId itemId = game.addItemToInventory("iron_sword");
    const UnitId first = game.addUnitToBench(unitFromDefinition("aster_vanguard"));
    assert(game.equipItem(itemId, first).success);
    game.addUnitToBench(unitFromDefinition("aster_vanguard"));
    const UnitId newest = game.addUnitToBench(unitFromDefinition("aster_vanguard"));

    Unit* kept = game.unit(newest);
    assert(kept != nullptr);
    assert(kept->star() == 2);
    assert(kept->placement().kind == PlacementKind::BenchSlot);
    assert(*kept->placement().benchSlot == 2);
    assert(game.unit(first) == nullptr);

    const auto inventory = game.equipmentInventory();
    assert(inventory.size() == 1);
    assert(inventory.front().itemId == itemId);
}

void testRepeatedRecomputeDoesNotRepeatHpDeltaAndStarStats() {
    GameState game(8, 8, 8, stage3Config());
    const UnitId first = game.addUnitToBench(unitFromDefinition("rowan_ranger"));
    game.addUnitToBench(unitFromDefinition("rowan_ranger"));
    const UnitId kept = game.addUnitToBench(unitFromDefinition("rowan_ranger"));
    assert(game.unit(first) == nullptr);

    Unit* unit = game.unit(kept);
    assert(unit != nullptr);
    const UnitStats base = unit->baseStats();
    assert(unit->star() == 2);
    assert(unit->maxHp() == static_cast<int>(std::lround(base.maxHp * 1.7)));
    assert(unit->atk() == static_cast<int>(std::lround(base.atk * 1.7)));
    assert(unit->range() == base.range);
    assert(unit->maxMana() == base.maxMana);

    const int hpAfterMerge = unit->hp();
    game.computeEffectiveStats(kept);
    game.computeEffectiveStats(kept);
    assert(unit->hp() == hpAfterMerge);
}

void testSynergyDedupAndCombatFreeze() {
    GameState game(8, 8, 8, stage3Config());
    game.addUnitToBench(unitFromDefinition("aster_vanguard"));
    game.addUnitToBench(unitFromDefinition("aster_vanguard"));
    assert(game.deployFromBench(0, Position{7, 0}));
    assert(game.deployFromBench(1, Position{7, 1}));
    const SynergyStatus* warriorDedup = findSynergy(game, "Warrior");
    assert(warriorDedup != nullptr);
    assert(warriorDedup->count == 1);
    assert(!warriorDedup->active);

    GameState combatGame(8, 8, 8, stage3Config());
    const UnitId warriorA = combatGame.addUnitToBench(unitFromDefinition("aster_vanguard"));
    const UnitId warriorB = combatGame.addUnitToBench(unitFromDefinition("mira_spark"));
    assert(combatGame.deployFromBench(0, Position{7, 0}));
    assert(combatGame.deployFromBench(1, Position{7, 1}));
    assert(combatGame.startCombat().success);
    const SynergyStatus* frozenWarrior = findSynergy(combatGame, "Warrior");
    assert(frozenWarrior != nullptr && frozenWarrior->active && frozenWarrior->count == 2);
    combatGame.unit(warriorA)->setHp(0);
    combatGame.unit(warriorB)->setHp(0);
    frozenWarrior = findSynergy(combatGame, "Warrior");
    assert(frozenWarrior != nullptr && frozenWarrior->active && frozenWarrior->count == 2);
}

void testEquipmentStats() {
    GameState game(8, 8, 8, stage3Config());
    const UnitId crystalTarget = game.addUnitToBench(unitFromDefinition("aster_vanguard"));
    game.unit(crystalTarget)->setMana(60);
    const ItemId crystal = game.addItemToInventory("blue_crystal");
    assert(game.equipItem(crystal, crystalTarget).success);
    assert(game.unit(crystalTarget)->maxMana() == 30);
    assert(game.unit(crystalTarget)->mana() == 30);

    const UnitId gloveTarget = game.addUnitToBench(unitFromDefinition("iris_guard"));
    const UnitId untouched = game.addUnitToBench(unitFromDefinition("selene_mystic"));
    const int gloveBase = game.unit(gloveTarget)->attackInterval();
    const int untouchedBase = game.unit(untouched)->attackInterval();
    const ItemId glove = game.addItemToInventory("swift_gloves");
    assert(game.equipItem(glove, gloveTarget).success);
    assert(game.unit(gloveTarget)->attackInterval() < gloveBase);
    assert(game.unit(untouched)->attackInterval() == untouchedBase);
}

void writeInvalidDefinitionSave(const std::string& path) {
    std::ofstream out(path);
    out << "SYNERA_SAVE 1\n"
        << "BOARD 8 8\n"
        << "BENCH 8\n"
        << "PLAYER 100 5 1 3 1\n"
        << "STATE Prep None Ongoing 2 1 2\n"
        << "SHOP 5\n"
        << "OFFER 0 EMPTY 0\n"
        << "OFFER 1 EMPTY 0\n"
        << "OFFER 2 EMPTY 0\n"
        << "OFFER 3 EMPTY 0\n"
        << "OFFER 4 EMPTY 0\n"
        << "ITEMS 0\n"
        << "UNITS 1\n"
        << "UNIT 1 missing_definition PlayerCtrl 1 1 100 0 0 NONE -1 -1 -1\n"
        << "SNAPSHOTS 0\n"
        << "END\n";
}

void writeConflictingPlacementSave(const std::string& path) {
    std::ofstream out(path);
    out << "SYNERA_SAVE 1\n"
        << "BOARD 8 8\n"
        << "BENCH 8\n"
        << "PLAYER 100 5 1 3 1\n"
        << "STATE Prep None Ongoing 3 1 3\n"
        << "SHOP 5\n"
        << "OFFER 0 EMPTY 0\n"
        << "OFFER 1 EMPTY 0\n"
        << "OFFER 2 EMPTY 0\n"
        << "OFFER 3 EMPTY 0\n"
        << "OFFER 4 EMPTY 0\n"
        << "ITEMS 0\n"
        << "UNITS 2\n"
        << "UNIT 1 aster_vanguard PlayerCtrl 1 1 420 0 0 BOARD -1 7 0\n"
        << "UNIT 2 mira_spark PlayerCtrl 1 2 260 0 0 BOARD -1 7 0\n"
        << "SNAPSHOTS 0\n"
        << "END\n";
}

void testLoadFailureLeavesOriginalStateUntouched() {
    GameState game(8, 8, 8, stage3Config());
    game.player().setGold(17);
    const UnitId existing = game.addUnitToBench(unitFromDefinition("iris_guard"));
    assert(game.unit(existing) != nullptr);

    writeInvalidDefinitionSave("stage3_bad_def.sav");
    assert(!game.loadFromFile("stage3_bad_def.sav").success);
    assert(game.player().gold() == 17);
    assert(game.unit(existing) != nullptr);

    writeConflictingPlacementSave("stage3_bad_placement.sav");
    assert(!game.loadFromFile("stage3_bad_placement.sav").success);
    assert(game.player().gold() == 17);
    assert(game.unit(existing) != nullptr);

    std::remove("stage3_bad_def.sav");
    std::remove("stage3_bad_placement.sav");
}

void testSaveLoadRoundTripRestoresAuthoritativeState() {
    GameState game(8, 8, 8, stage3Config());
    game.player().setGold(11);
    const UnitId unitId = game.addUnitToBench(unitFromDefinition("aster_vanguard"));
    const ItemId itemId = game.addItemToInventory("chainmail");
    assert(game.equipItem(itemId, unitId).success);
    assert(game.deployFromBench(0, Position{7, 4}));
    assert(game.saveToFile("stage3_roundtrip.sav").success);

    GameState loaded(4, 4, 2, stage3Config());
    assert(loaded.loadFromFile("stage3_roundtrip.sav").success);
    assert(loaded.board().rows() == 8);
    assert(loaded.bench().capacity() == 8);
    assert(loaded.player().gold() == 11);
    assert(loaded.board().occupant(Position{7, 4}) == unitId);
    const Unit* loadedUnit = loaded.unit(unitId);
    assert(loadedUnit != nullptr);
    assert(loadedUnit->equippedItemId() == itemId);
    assert(loadedUnit->maxHp() == loadedUnit->baseStats().maxHp + 150);

    std::remove("stage3_roundtrip.sav");
}

}  // namespace

int main() {
    testInitialShopPurchaseAndRefresh();
    testBusinessApisArePrepOnlyAndCombatSaveRejected();
    testPopulationLimitAndUpgrade();
    testMergeKeepsNewestAndReturnsRemovedEquipment();
    testRepeatedRecomputeDoesNotRepeatHpDeltaAndStarStats();
    testSynergyDedupAndCombatFreeze();
    testEquipmentStats();
    testLoadFailureLeavesOriginalStateUntouched();
    testSaveLoadRoundTripRestoresAuthoritativeState();
    return 0;
}
