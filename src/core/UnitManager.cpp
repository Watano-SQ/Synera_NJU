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

UnitId UnitManager::addWithId(UnitId id, std::unique_ptr<Unit> unit) {
    if (!unit) {
        throw std::invalid_argument("Cannot add a null unit.");
    }
    if (id == 0 || units_.find(id) != units_.end()) {
        throw std::invalid_argument("Unit id must be unique and non-zero.");
    }
    units_.emplace(id, std::move(unit));
    nextId_ = std::max(nextId_, id + 1);
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

bool UnitManager::contains(UnitId id) const {
    return units_.find(id) != units_.end();
}

bool UnitManager::remove(UnitId id) {
    return units_.erase(id) > 0;
}

void UnitManager::clear() {
    units_.clear();
    nextId_ = 1;
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

UnitId UnitManager::nextId() const {
    return nextId_;
}

void UnitManager::setNextId(UnitId nextId) {
    nextId_ = std::max<UnitId>(1, nextId);
}

}  // namespace synera
