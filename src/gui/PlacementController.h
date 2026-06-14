#pragma once

#include "DragData.h"
#include "core/GameState.h"

#include <QString>

namespace synera::gui {

enum class GuiPhase {
    Prep,
    Combat,
    Resolve
};

struct PlacementResult {
    bool success = false;
    QString message;
};

class PlacementController {
public:
    explicit PlacementController(GameState* game);

    GuiPhase phase() const;
    void setPhase(GuiPhase phase);
    bool canDrag(UnitId id) const;

    PlacementResult dropOnBoard(const UnitDragData& dragData, Position target);
    PlacementResult dropOnBench(const UnitDragData& dragData, int targetSlot);

private:
    PlacementResult reject(QString message = "Invalid placement") const;
    bool isPlayerUnit(UnitId id) const;

    GameState* game_;
    GuiPhase phase_ = GuiPhase::Prep;
};

}  // namespace synera::gui
