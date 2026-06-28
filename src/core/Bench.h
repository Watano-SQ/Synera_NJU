#pragma once

#include "Types.h"

#include <optional>
#include <vector>

namespace synera {

class GameState;

// 备战区是玩家单位的线性槽位表。敌人不能进入 Bench，这个约束由 GameState 守住。
class Bench {
public:
    explicit Bench(std::size_t capacity = 8);

    std::size_t capacity() const;
    bool isValidSlot(std::size_t slot) const;
    bool isEmpty(std::size_t slot) const;
    std::optional<std::size_t> firstEmptySlot() const;
    std::optional<UnitId> occupant(std::size_t slot) const;
    std::optional<UnitId> occupant(int slot) const;

private:
    friend class GameState;

    // 与 Board 类似，只有 GameState 能改槽位占用，避免 Unit 位置和槽位表不同步。
    void setOccupant(std::size_t slot, UnitId id);
    void clear(std::size_t slot);

    std::vector<std::optional<UnitId>> slots_;
};

}  // namespace synera
