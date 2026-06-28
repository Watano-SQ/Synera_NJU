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

    // 棋盘尺寸在构造后固定，所有坐标检查都以这两个值为准。
    int rows() const;
    int cols() const;
    // isInside 是调用其它坐标函数前最基础的防线，避免访问 cells_ 时越界。
    bool isInside(Position position) const;
    // 约定敌人在上半场、玩家在下半场；部署和敌人生成都会复用这两个判断。
    bool isPlayerHalf(Position position) const;
    bool isEnemyHalf(Position position) const;
    // isEmpty/occupant 只说明占用情况，不说明单位是否存活；死亡清理由 GameState 完成。
    bool isEmpty(Position position) const;
    // canPlace 会同时检查边界、空位和单位归属对应的半场限制。
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
    // 一维数组按 row-major 存储：index = row * cols_ + col。
    // 这里保存 optional<UnitId> 而非 Unit*，可以让 UnitManager 独立负责对象生命周期。
    std::vector<std::optional<UnitId>> cells_;
};

}  // namespace synera
