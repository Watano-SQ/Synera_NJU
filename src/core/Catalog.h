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

const std::vector<ItemDefinition>& itemCatalog();
const ItemDefinition* findItemDefinition(const std::string& itemDefId);

}  // namespace synera
