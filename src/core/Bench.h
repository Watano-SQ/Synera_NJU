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

    // Bench 容量固定，升级人口不会扩大 Bench；人口只限制棋盘上的玩家单位数量。
    std::size_t capacity() const;
    // 任何来自 GUI 的槽位都先走 isValidSlot，防止负数转 size_t 后变成巨大下标。
    bool isValidSlot(std::size_t slot) const;
    bool isEmpty(std::size_t slot) const;
    // 购买商店单位时优先放入第一个空槽，保证行为可预测、测试可复现。
    std::optional<std::size_t> firstEmptySlot() const;
    std::optional<UnitId> occupant(std::size_t slot) const;
    // int 重载服务 GUI 命中测试结果；内部仍会转换成 size_t 并做有效性检查。
    std::optional<UnitId> occupant(int slot) const;

private:
    friend class GameState;

    // 与 Board 类似，只有 GameState 能改槽位占用，避免 Unit 位置和槽位表不同步。
    void setOccupant(std::size_t slot, UnitId id);
    void clear(std::size_t slot);

    // slots_ 与 Board::cells_ 一样只保存 UnitId，避免同一个 Unit 被多个容器共同拥有。
    std::vector<std::optional<UnitId>> slots_;
};

}  // namespace synera
