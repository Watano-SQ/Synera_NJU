#pragma once

#include "Types.h"
#include "Unit.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace synera {

class UnitManager {
public:
    UnitId add(std::unique_ptr<Unit> unit);
    UnitId addWithId(UnitId id, std::unique_ptr<Unit> unit);
    Unit* get(UnitId id);
    const Unit* get(UnitId id) const;
    bool contains(UnitId id) const;
    bool remove(UnitId id);
    void clear();
    std::vector<UnitId> ids() const;
    UnitId nextId() const;
    void setNextId(UnitId nextId);

private:
    UnitId nextId_ = 1;
    std::unordered_map<UnitId, std::unique_ptr<Unit>> units_;
};

}  // namespace synera
