#include "UnitManager.h"

#include <algorithm>
#include <stdexcept>

namespace synera {

UnitId UnitManager::add(std::unique_ptr<Unit> unit) {
    if (!unit) {
        throw std::invalid_argument("Cannot add a null unit.");
    }
    const UnitId id = nextId_++;
    units_.emplace(id, std::move(unit));
    return id;
}

Unit* UnitManager::get(UnitId id) {
    const auto it = units_.find(id);
    if (it == units_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const Unit* UnitManager::get(UnitId id) const {
    const auto it = units_.find(id);
    if (it == units_.end()) {
        return nullptr;
    }
    return it->second.get();
}

bool UnitManager::remove(UnitId id) {
    return units_.erase(id) > 0;
}

std::vector<UnitId> UnitManager::ids() const {
    std::vector<UnitId> result;
    result.reserve(units_.size());
    for (const auto& [id, unit] : units_) {
        result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

}  // namespace synera
