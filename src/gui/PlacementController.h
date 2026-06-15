#pragma once

#include "DragData.h"
#include "core/GameState.h"

#include <QString>

namespace synera::gui {

struct PlacementResult {
    bool success = false;
    QString message;
};

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
