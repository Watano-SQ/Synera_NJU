#pragma once

#include "DragData.h"
#include "core/GameState.h"

#include <QString>

namespace synera::gui {

// GUI 放置操作的统一返回值，方便 MainWindow 把结果显示到状态栏。
struct PlacementResult {
    bool success = false;
    QString message;
};

// 把 GUI 拖拽动作翻译成 GameState 的部署、移动和回 Bench 命令。
class PlacementController {
public:
    explicit PlacementController(GameState* game);

    // phase/canDrag 提供给 BoardWidget 与 BenchWidget 判断当前是否允许启动拖拽。
    // 控件层只做体验层面的启用/禁用，真正的规则校验仍在 dropOnBoard/dropOnBench 内重复执行。
    GamePhase phase() const;
    bool canDrag(UnitId id) const;

    // 两个 drop 入口是 GUI 拖拽到规则层命令的翻译点。
    // 返回值包含面向状态栏的英文短消息，方便用户知道是移动、交换还是被规则拒绝。
    PlacementResult dropOnBoard(const UnitDragData& dragData, Position target);
    PlacementResult dropOnBench(const UnitDragData& dragData, int targetSlot);

private:
    // reject 统一构造失败结果，避免每个分支手写 success=false。
    PlacementResult reject(QString message = "Invalid placement") const;
    // 所有拖拽只允许玩家单位，敌方单位即使被误点中也不能被玩家移动。
    bool isPlayerUnit(UnitId id) const;

    // 不拥有 GameState；MainWindow 负责保证 PlacementController 生命周期短于 game_。
    GameState* game_;
};

}  // namespace synera::gui
