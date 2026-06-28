#pragma once

#include "Types.h"
#include "Unit.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace synera {

// UnitManager 拥有所有 Unit 对象。Board 和 Bench 只保存这里分配出的 UnitId。
class UnitManager {
public:
    UnitId add(std::unique_ptr<Unit> unit);
    // 读档时需要恢复原 ID，所以单独提供 addWithId。
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
    // nextId_ 始终指向下一个可自动分配的非零 ID。
    UnitId nextId_ = 1;
    std::unordered_map<UnitId, std::unique_ptr<Unit>> units_;
};

}  // namespace synera
