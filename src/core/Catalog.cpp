#include "Catalog.h"

#include <algorithm>

namespace synera {

namespace {

UnitDefinition makeDefinition(std::string definitionId,
                              std::string name,
                              int cost,
                              std::vector<std::string> traits,
                              UnitStats stats,
                              std::string factoryKey,
                              std::string visualKey) {
    UnitDefinition definition;
    definition.definitionId = std::move(definitionId);
    definition.name = std::move(name);
    definition.cost = cost;
    definition.traits = std::move(traits);
    definition.baseStats = stats;
    definition.factoryKey = std::move(factoryKey);
    definition.visualKey = std::move(visualKey);
    return definition;
}

}  // namespace

const std::vector<UnitDefinition>& unitCatalog() {
    static const std::vector<UnitDefinition> definitions{
        makeDefinition("aster_vanguard",
                       "Aster Vanguard",
                       2,
                       {"Warrior", "Guardian"},
                       UnitStats{420, 38, 1, 60, 60, 20},
                       "Vanguard",
                       "player_vanguard"),
        makeDefinition("mira_spark",
                       "Mira Spark",
                       3,
                       {"Mystic", "Warrior"},
                       UnitStats{260, 32, 2, 60, 60, 20},
                       "SparkMage",
                       "player_mage"),
        makeDefinition("iris_guard",
                       "Iris Guard",
                       2,
                       {"Guardian", "Healer"},
                       UnitStats{360, 28, 1, 60, 60, 20},
                       "IrisGuard",
                       "player_guard"),
        makeDefinition("rowan_ranger",
                       "Rowan Ranger",
                       2,
                       {"Ranger", "Warrior"},
                       UnitStats{300, 34, 3, 60, 54, 20},
                       "BasicUnit",
                       "player_ranger"),
        makeDefinition("selene_mystic",
                       "Selene Mystic",
                       3,
                       {"Mystic", "Healer"},
                       UnitStats{280, 30, 2, 70, 60, 20},
                       "SparkMage",
                       "player_mystic"),
        makeDefinition("thorn_warden",
                       "Thorn Warden",
                       2,
                       {"Guardian", "Ranger"},
                       UnitStats{380, 30, 2, 60, 60, 20},
                       "BasicUnit",
                       "player_warden"),
    };
    return definitions;
}

const UnitDefinition* findUnitDefinition(const std::string& definitionId) {
    const auto& definitions = unitCatalog();
    const auto it = std::find_if(definitions.begin(), definitions.end(), [&](const UnitDefinition& definition) {
        return definition.definitionId == definitionId;
    });
    return it == definitions.end() ? nullptr : &*it;
}

std::unique_ptr<Unit> createUnitFromDefinition(const UnitDefinition& definition, Owner owner) {
    if (definition.factoryKey == "Vanguard") {
        return std::make_unique<Vanguard>(definition.definitionId,
                                          definition.name,
                                          owner,
                                          definition.baseStats,
                                          definition.traits,
                                          definition.visualKey,
                                          definition.factoryKey);
    }
    if (definition.factoryKey == "SparkMage") {
        return std::make_unique<SparkMage>(definition.definitionId,
                                           definition.name,
                                           owner,
                                           definition.baseStats,
                                           definition.traits,
                                           definition.visualKey,
                                           definition.factoryKey);
    }
    if (definition.factoryKey == "IrisGuard") {
        return std::make_unique<IrisGuard>(definition.definitionId,
                                           definition.name,
                                           owner,
                                           definition.baseStats,
                                           definition.traits,
                                           definition.visualKey,
                                           definition.factoryKey);
    }
    return std::make_unique<BasicUnit>(definition.definitionId,
                                       definition.name,
                                       owner,
                                       definition.baseStats,
                                       definition.traits,
                                       definition.visualKey,
                                       definition.factoryKey);
}

const std::vector<ItemDefinition>& itemCatalog() {
    static const std::vector<ItemDefinition> definitions{
        ItemDefinition{"iron_sword", "Iron Sword", ItemEffectType::Attack, 15},
        ItemDefinition{"chainmail", "Chainmail", ItemEffectType::MaxHp, 150},
        ItemDefinition{"swift_gloves", "Swift Gloves", ItemEffectType::AttackIntervalPercent, -20},
        ItemDefinition{"blue_crystal", "Blue Crystal", ItemEffectType::MaxMana, -30},
    };
    return definitions;
}

const ItemDefinition* findItemDefinition(const std::string& itemDefId) {
    const auto& definitions = itemCatalog();
    const auto it = std::find_if(definitions.begin(), definitions.end(), [&](const ItemDefinition& definition) {
        return definition.itemDefId == itemDefId;
    });
    return it == definitions.end() ? nullptr : &*it;
}

}  // namespace synera
