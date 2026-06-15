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
        makeDefinition("peashooter",
                       "豌豆射手",
                       1,
                       {"shooter"},
                       UnitStats{300, 32, 3, 60, 60, 20},
                       "BasicUnit",
                       "units/peashooter"),
        makeDefinition("repeater",
                       "双重射手",
                       3,
                       {"shooter"},
                       UnitStats{320, 36, 3, 60, 56, 20},
                       "PeaBurst",
                       "units/repeater"),
        makeDefinition("sunflower",
                       "向日葵",
                       2,
                       {"sun", "healer"},
                       UnitStats{260, 22, 2, 60, 64, 20},
                       "SunHealer",
                       "units/sunflower"),
        makeDefinition("twin_sunflower",
                       "双胞向日葵",
                       3,
                       {"sun", "healer"},
                       UnitStats{300, 26, 2, 55, 62, 20},
                       "SunHealer",
                       "units/twin_sunflower"),
        makeDefinition("wallnut",
                       "坚果墙",
                       2,
                       {"nut"},
                       UnitStats{520, 18, 1, 70, 70, 20},
                       "BasicUnit",
                       "units/wallnut"),
        makeDefinition("tallnut",
                       "高坚果",
                       3,
                       {"nut"},
                       UnitStats{700, 20, 1, 80, 72, 20},
                       "BasicUnit",
                       "units/tallnut"),
        makeDefinition("puffshroom",
                       "小喷菇",
                       1,
                       {"fungus"},
                       UnitStats{220, 24, 2, 45, 48, 20},
                       "BasicUnit",
                       "units/puffshroom"),
        makeDefinition("fumeshroom",
                       "大喷菇",
                       2,
                       {"fungus", "shooter"},
                       UnitStats{320, 30, 2, 60, 60, 20},
                       "FumeLineCaster",
                       "units/fumeshroom"),
        makeDefinition("gloomshroom",
                       "忧郁菇",
                       3,
                       {"fungus"},
                       UnitStats{380, 34, 2, 65, 58, 20},
                       "FumeLineCaster",
                       "units/gloomshroom"),
        makeDefinition("spikeweed",
                       "地刺",
                       2,
                       {"spike"},
                       UnitStats{300, 34, 1, 60, 50, 20},
                       "BasicUnit",
                       "units/spikeweed"),
        makeDefinition("spikerock",
                       "地刺王",
                       3,
                       {"spike", "nut"},
                       UnitStats{430, 38, 1, 60, 52, 20},
                       "BasicUnit",
                       "units/spikerock"),
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
    if (definition.factoryKey == "PeaBurst") {
        return std::make_unique<PeaBurst>(definition.definitionId,
                                          definition.name,
                                          owner,
                                          definition.baseStats,
                                          definition.traits,
                                          definition.visualKey,
                                          definition.factoryKey);
    }
    if (definition.factoryKey == "FumeLineCaster") {
        return std::make_unique<FumeLineCaster>(definition.definitionId,
                                                definition.name,
                                                owner,
                                                definition.baseStats,
                                                definition.traits,
                                                definition.visualKey,
                                                definition.factoryKey);
    }
    if (definition.factoryKey == "SunHealer") {
        return std::make_unique<SunHealer>(definition.definitionId,
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
        ItemDefinition{"plant_food", "植物营养剂", "items/plant_food", ItemEffectType::Attack, 15},
        ItemDefinition{"pumpkin_shell", "南瓜护壳", "items/pumpkin_shell", ItemEffectType::MaxHp, 150},
        ItemDefinition{"garden_glove", "园艺手套", "items/garden_glove", ItemEffectType::AttackIntervalPercent, -20},
        ItemDefinition{"chlorophyll", "叶绿素", "items/chlorophyll", ItemEffectType::MaxMana, -30},
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

const std::vector<TraitDefinition>& traitCatalog() {
    static const std::vector<TraitDefinition> definitions{
        TraitDefinition{"shooter", "射手", "traits/shooter", "射手植物提高攻击频率。"},
        TraitDefinition{"nut", "坚果", "traits/nut", "坚果单位获得额外生命值。"},
        TraitDefinition{"sun", "阳光", "traits/sun", "胜利结算时获得额外阳光。"},
        TraitDefinition{"healer", "治愈", "traits/healer", "治疗效果提高。"},
        TraitDefinition{"fungus", "真菌", "traits/fungus", "真菌单位更快释放技能。"},
        TraitDefinition{"spike", "地刺", "traits/spike", "地刺单位获得攻击力。"},
    };
    return definitions;
}

const TraitDefinition* findTraitDefinition(const std::string& traitId) {
    const auto& definitions = traitCatalog();
    const auto it = std::find_if(definitions.begin(), definitions.end(), [&](const TraitDefinition& definition) {
        return definition.traitId == traitId;
    });
    return it == definitions.end() ? nullptr : &*it;
}

}  // namespace synera
