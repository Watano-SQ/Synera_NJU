#include "core/Catalog.h"
#include "core/EncounterGenerator.h"
#include "core/GameState.h"
#include "core/Unit.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>
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

const std::unordered_set<std::string>& evolvedVisualOnlyIds() {
    static const std::unordered_set<std::string> ids{
        "repeater", "twin_sunflower", "tallnut", "scaredyshroom", "gloomshroom", "spikerock"};
    return ids;
}

std::unique_ptr<Unit> sunTestUnit(const std::string& definitionId) {
    return std::make_unique<BasicUnit>(definitionId,
                                       Owner::PlayerCtrl,
                                       500,
                                       40,
                                       10,
                                       60,
                                       std::vector<std::string>{"sun"},
                                       "units/sunflower");
}

const SynergyStatus* findSynergy(const GameState& game, const std::string& trait) {
    for (const SynergyStatus& status : game.activeSynergies()) {
        if (status.trait == trait) {
            return &status;
        }
    }
    return nullptr;
}

std::filesystem::path findAssetRoot() {
    std::filesystem::path path = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        if (std::filesystem::exists(path / "assets")) {
            return path / "assets";
        }
        path = path.parent_path();
    }
    return std::filesystem::current_path() / "assets";
}

bool assetExists(const std::filesystem::path& assetRoot, const std::string& visualKey) {
    return std::filesystem::exists(assetRoot / (visualKey + ".png")) ||
           std::filesystem::exists(assetRoot / (visualKey + ".jpg")) ||
           std::filesystem::exists(assetRoot / (visualKey + ".jpeg"));
}

void testInitialShopPurchaseAndRefresh() {
    GameState game(8, 8, 8, stage3Config());
    assert(game.shopOffers().size() == 5);
    for (const auto& offer : game.shopOffers()) {
        assert(offer.has_value());
        assert(evolvedVisualOnlyIds().find(offer->definitionId) == evolvedVisualOnlyIds().end());
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
        assert(evolvedVisualOnlyIds().find(offer->definitionId) == evolvedVisualOnlyIds().end());
    }
}

void testBusinessApisArePrepOnlyAndCombatSaveRejected() {
    GameState game(8, 8, 8, stage3Config());
    game.player().setGold(20);
    game.addItemToInventory("plant_food");
    const UnitId unitId = game.addUnitToBench(unitFromDefinition("peashooter"));
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
    game.addUnitToBench(unitFromDefinition("peashooter"));
    game.addUnitToBench(unitFromDefinition("sunflower"));
    game.addUnitToBench(unitFromDefinition("wallnut"));
    game.addUnitToBench(unitFromDefinition("puffshroom"));

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
    const ItemId itemId = game.addItemToInventory("plant_food");
    const UnitId first = game.addUnitToBench(unitFromDefinition("peashooter"));
    assert(game.equipItem(itemId, first).success);
    game.addUnitToBench(unitFromDefinition("peashooter"));
    const UnitId newest = game.addUnitToBench(unitFromDefinition("peashooter"));

    Unit* kept = game.unit(newest);
    assert(kept != nullptr);
    assert(kept->definitionId() == "peashooter");
    assert(kept->star() == 2);
    assert(displayVisualKey(*kept) == "units/repeater");
    assert(kept->factoryKey() == "BasicUnit");
    assert(kept->traits().size() == 1 && kept->traits().front() == "shooter");
    assert(kept->placement().kind == PlacementKind::BenchSlot);
    assert(*kept->placement().benchSlot == 2);
    assert(game.unit(first) == nullptr);

    const auto inventory = game.equipmentInventory();
    assert(inventory.size() == 1);
    assert(inventory.front().itemId == itemId);
}

void testRepeatedRecomputeDoesNotRepeatHpDeltaAndStarStats() {
    GameState game(8, 8, 8, stage3Config());
    const UnitId first = game.addUnitToBench(unitFromDefinition("spikeweed"));
    game.addUnitToBench(unitFromDefinition("spikeweed"));
    const UnitId kept = game.addUnitToBench(unitFromDefinition("spikeweed"));
    assert(game.unit(first) == nullptr);

    Unit* unit = game.unit(kept);
    assert(unit != nullptr);
    const UnitStats base = unit->baseStats();
    assert(unit->star() == 2);
    assert(unit->definitionId() == "spikeweed");
    assert(unit->factoryKey() == "BasicUnit");
    assert(unit->traits().size() == 1 && unit->traits().front() == "spike");
    assert(displayVisualKey(*unit) == "units/spikerock");
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
    game.addUnitToBench(unitFromDefinition("peashooter"));
    game.addUnitToBench(unitFromDefinition("peashooter"));
    assert(game.deployFromBench(0, Position{7, 0}));
    assert(game.deployFromBench(1, Position{7, 1}));
    const SynergyStatus* shooterDedup = findSynergy(game, "shooter");
    assert(shooterDedup != nullptr);
    assert(shooterDedup->count == 1);
    assert(!shooterDedup->active);

    GameState combatGame(8, 8, 8, stage3Config());
    const UnitId shooterA = combatGame.addUnitToBench(unitFromDefinition("peashooter"));
    const UnitId shooterB = combatGame.addUnitToBench(unitFromDefinition("fumeshroom"));
    assert(combatGame.deployFromBench(0, Position{7, 0}));
    assert(combatGame.deployFromBench(1, Position{7, 1}));
    assert(combatGame.startCombat().success);
    const SynergyStatus* frozenShooter = findSynergy(combatGame, "shooter");
    assert(frozenShooter != nullptr && frozenShooter->active && frozenShooter->count == 2);
    combatGame.unit(shooterA)->setHp(0);
    combatGame.unit(shooterB)->setHp(0);
    frozenShooter = findSynergy(combatGame, "shooter");
    assert(frozenShooter != nullptr && frozenShooter->active && frozenShooter->count == 2);
}

void testEquipmentStats() {
    GameState game(8, 8, 8, stage3Config());
    const UnitId crystalTarget = game.addUnitToBench(unitFromDefinition("peashooter"));
    game.unit(crystalTarget)->setMana(60);
    const ItemId crystal = game.addItemToInventory("chlorophyll");
    assert(game.equipItem(crystal, crystalTarget).success);
    assert(game.unit(crystalTarget)->maxMana() == 30);
    assert(game.unit(crystalTarget)->mana() == 30);

    const UnitId gloveTarget = game.addUnitToBench(unitFromDefinition("sunflower"));
    const UnitId untouched = game.addUnitToBench(unitFromDefinition("puffshroom"));
    const int gloveBase = game.unit(gloveTarget)->attackInterval();
    const int untouchedBase = game.unit(untouched)->attackInterval();
    const ItemId glove = game.addItemToInventory("garden_glove");
    assert(game.equipItem(glove, gloveTarget).success);
    assert(game.unit(gloveTarget)->attackInterval() < gloveBase);
    assert(game.unit(untouched)->attackInterval() == untouchedBase);

    const UnitId shellTarget = game.addUnitToBench(unitFromDefinition("wallnut"));
    const int shellBase = game.unit(shellTarget)->maxHp();
    const ItemId shell = game.addItemToInventory("pumpkin_shell");
    assert(game.equipItem(shell, shellTarget).success);
    assert(game.unit(shellTarget)->maxHp() == shellBase + 150);
}

void testSunSynergyAddsOnlyVictoryGold() {
    GameState victoryGame(8, 8, 8, stage3Config());
    victoryGame.player().setGold(0);
    const UnitId sunflower = victoryGame.addUnitToBench(unitFromDefinition("sunflower"));
    victoryGame.addUnitToBench(sunTestUnit("sun_helper"));
    victoryGame.addUnitToBench(unitFromDefinition("peashooter"));
    assert(victoryGame.deployFromBench(0, Position{7, 0}));
    assert(victoryGame.deployFromBench(1, Position{7, 1}));
    assert(victoryGame.deployFromBench(2, Position{7, 2}));
    victoryGame.unit(sunflower)->setBaseStats(UnitStats{500, 999, 10, 60, 1, 1});
    assert(victoryGame.startCombat().success);
    assert(victoryGame.tickCombat().success);
    assert(victoryGame.phase() == GamePhase::Resolve);
    assert(victoryGame.resolveRound().success);
    assert(victoryGame.player().gold() == stage3Config().victoryGold + 1);

    GameState defeatGame(8, 8, 8, stage3Config());
    defeatGame.player().setGold(0);
    defeatGame.player().setCurrentRound(1);
    defeatGame.addUnitToBench(unitFromDefinition("sunflower"));
    defeatGame.addUnitToBench(sunTestUnit("sun_helper"));
    assert(defeatGame.startCombat().success);
    assert(defeatGame.phase() == GamePhase::Resolve);
    assert(defeatGame.resolveRound().success);
    assert(defeatGame.player().gold() == stage3Config().defeatGold);
}

void testCatalogAndAssetKeysAreValid() {
    assert(findUnitDefinition("peashooter") != nullptr);
    assert(findUnitDefinition("sunflower") != nullptr);
    assert(findItemDefinition("plant_food") != nullptr);
    assert(findTraitDefinition("shooter") != nullptr);
    for (const std::string& evolvedId : evolvedVisualOnlyIds()) {
        assert(findUnitDefinition(evolvedId) == nullptr);
    }

    assert(boardHalfBackgroundVisualKey(BoardHalf::Enemy) == "backgrounds/night_board");
    assert(boardHalfBackgroundVisualKey(BoardHalf::Player) == "backgrounds/day_board");

    BasicUnit enemy("Enemy Visual",
                    Owner::EnemyCtrl,
                    100,
                    10,
                    1,
                    60,
                    std::vector<std::string>{"zombie"},
                    "enemies/custom_zombie");
    assert(displayVisualKey(enemy) == "enemies/custom_zombie");

    const std::filesystem::path assetRoot = findAssetRoot();
    for (const UnitDefinition& unit : unitCatalog()) {
        assert(assetExists(assetRoot, unit.visualKey));
        assert(assetExists(assetRoot, unit.star1VisualKey));
        assert(assetExists(assetRoot, unit.star2VisualKey));
        assert(assetExists(assetRoot, "shop_cards/" + unit.definitionId + "_available"));
        assert(assetExists(assetRoot, "shop_cards/" + unit.definitionId + "_disabled"));
    }
    assert(assetExists(assetRoot, "units/scaredyshroom"));
    assert(assetExists(assetRoot, "ui/zombieline"));
    assert(assetExists(assetRoot, boardHalfBackgroundVisualKey(BoardHalf::Enemy)));
    assert(assetExists(assetRoot, boardHalfBackgroundVisualKey(BoardHalf::Player)));
    for (const ItemDefinition& item : itemCatalog()) {
        assert(assetExists(assetRoot, item.visualKey));
    }
    for (const TraitDefinition& trait : traitCatalog()) {
        assert(assetExists(assetRoot, trait.visualKey));
    }

    std::unordered_set<std::string> enemyVisualKeys;
    for (int round = 1; round <= 5; ++round) {
        for (const EnemyTemplate& enemyTemplate : EncounterGenerator::templatesForRound(round)) {
            enemyVisualKeys.insert(enemyTemplate.visualKey);
        }
    }
    for (const std::string& visualKey : enemyVisualKeys) {
        assert(assetExists(assetRoot, visualKey));
    }
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
        << "UNIT 1 peashooter PlayerCtrl 1 1 300 0 0 BOARD -1 7 0\n"
        << "UNIT 2 sunflower PlayerCtrl 1 2 260 0 0 BOARD -1 7 0\n"
        << "SNAPSHOTS 0\n"
        << "END\n";
}

void testLoadFailureLeavesOriginalStateUntouched() {
    GameState game(8, 8, 8, stage3Config());
    game.player().setGold(17);
    const UnitId existing = game.addUnitToBench(unitFromDefinition("sunflower"));
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
    const UnitId unitId = game.addUnitToBench(unitFromDefinition("wallnut"));
    const ItemId itemId = game.addItemToInventory("pumpkin_shell");
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
    testSunSynergyAddsOnlyVictoryGold();
    testCatalogAndAssetKeysAreValid();
    testLoadFailureLeavesOriginalStateUntouched();
    testSaveLoadRoundTripRestoresAuthoritativeState();
    return 0;
}
