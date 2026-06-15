#pragma once

#include "Types.h"
#include "Unit.h"

#include <memory>
#include <string>
#include <vector>

namespace synera {

const std::vector<UnitDefinition>& unitCatalog();
const UnitDefinition* findUnitDefinition(const std::string& definitionId);
std::unique_ptr<Unit> createUnitFromDefinition(const UnitDefinition& definition, Owner owner);
std::string displayVisualKey(const Unit& unit);
std::string boardHalfBackgroundVisualKey(BoardHalf half);

const std::vector<ItemDefinition>& itemCatalog();
const ItemDefinition* findItemDefinition(const std::string& itemDefId);

const std::vector<TraitDefinition>& traitCatalog();
const TraitDefinition* findTraitDefinition(const std::string& traitId);

}  // namespace synera
