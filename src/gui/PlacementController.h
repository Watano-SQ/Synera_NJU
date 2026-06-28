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

    GamePhase phase() const;
    bool canDrag(UnitId id) const;

    PlacementResult dropOnBoard(const UnitDragData& dragData, Position target);
    PlacementResult dropOnBench(const UnitDragData& dragData, int targetSlot);

private:
    PlacementResult reject(QString message = "Invalid placement") const;
    bool isPlayerUnit(UnitId id) const;

    GameState* game_;
};

}  // namespace synera::gui
