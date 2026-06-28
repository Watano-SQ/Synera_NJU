#pragma once

#include "Types.h"
#include "Unit.h"

#include <optional>
#include <vector>

namespace synera {

class GameState;

// 棋盘只负责保存每个格子上的 UnitId 和半场边界判断。
// 它不拥有 Unit 对象，也不直接执行战斗、部署、交换等游戏规则。
class Board {
public:
    Board(int rows = 8, int cols = 8);

    int rows() const;
    int cols() const;
    bool isInside(Position position) const;
    bool isPlayerHalf(Position position) const;
    bool isEnemyHalf(Position position) const;
    bool isEmpty(Position position) const;
    bool canPlace(const Unit& unit, Position position) const;
    std::optional<UnitId> occupant(Position position) const;

private:
    friend class GameState;

    // 写入占用关系的函数保持 private，确保外部必须通过 GameState 维护 Unit::placement。
    std::size_t index(Position position) const;
    void setOccupant(Position position, UnitId id);
    void clear(Position position);

    int rows_;
    int cols_;
    std::vector<std::optional<UnitId>> cells_;
};

}  // namespace synera
