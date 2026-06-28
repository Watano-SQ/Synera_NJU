#pragma once

#include "Types.h"
#include "Unit.h"

#include <memory>
#include <string>
#include <vector>

namespace synera {

// 目录层保存所有可购买单位的静态数据，GameState 通过 definitionId 查找并创建实例。
const std::vector<UnitDefinition>& unitCatalog();
const UnitDefinition* findUnitDefinition(const std::string& definitionId);
std::unique_ptr<Unit> createUnitFromDefinition(const UnitDefinition& definition, Owner owner);
// 根据单位星级和归属选择实际显示资源。
std::string displayVisualKey(const Unit& unit);
std::string boardHalfBackgroundVisualKey(BoardHalf half);

// 装备和羁绊目录用于属性计算、掉落和 GUI 展示。
const std::vector<ItemDefinition>& itemCatalog();
const ItemDefinition* findItemDefinition(const std::string& itemDefId);

const std::vector<TraitDefinition>& traitCatalog();
const TraitDefinition* findTraitDefinition(const std::string& traitId);

}  // namespace synera
