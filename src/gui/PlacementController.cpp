#include "PlacementController.h"

namespace synera::gui {

PlacementController::PlacementController(GameState* game) : game_(game) {}

GamePhase PlacementController::phase() const {
    return game_ != nullptr ? game_->phase() : GamePhase::GameOver;
}

bool PlacementController::canDrag(UnitId id) const {
    return phase() == GamePhase::Prep && isPlayerUnit(id);
}

PlacementResult PlacementController::dropOnBoard(const UnitDragData& dragData, Position target) {
    if (!canDrag(dragData.unitId)) {
        return reject("Only player units can move during Prep");
    }
    if (!game_->board().isInside(target)) {
        return reject();
    }

    if (dragData.sourceType == DragSourceType::Bench) {
        if (dragData.benchSlot < 0) {
            return reject();
        }
        if (game_->board().occupant(target).has_value()) {
            return reject("Bench to occupied board cell is not supported yet");
        }
        const bool ok = game_->deployFromBench(static_cast<std::size_t>(dragData.benchSlot), target,
                                               PlacementPolicy::Reject);
        return {ok, ok ? "Unit deployed" : "Invalid placement"};
    }

    const auto targetOccupant = game_->board().occupant(target);
    PlacementPolicy policy = PlacementPolicy::Reject;
    if (targetOccupant.has_value()) {
        const Unit* targetUnit = game_->unit(*targetOccupant);
        if (targetUnit == nullptr || targetUnit->owner() != Owner::PlayerCtrl) {
            return reject();
        }
        policy = PlacementPolicy::Swap;
    }

    const bool ok = game_->moveBoardUnit(dragData.boardPosition, target, policy);
    return {ok, ok ? (policy == PlacementPolicy::Swap ? "Units swapped" : "Unit moved") : "Invalid placement"};
}

PlacementResult PlacementController::dropOnBench(const UnitDragData& dragData, int targetSlot) {
    if (!canDrag(dragData.unitId)) {
        return reject("Only player units can move during Prep");
    }
    if (!game_->bench().isValidSlot(static_cast<std::size_t>(targetSlot)) || targetSlot < 0) {
        return reject();
    }
    if (!game_->bench().isEmpty(static_cast<std::size_t>(targetSlot))) {
        return reject("Target bench slot is occupied");
    }
    if (dragData.sourceType != DragSourceType::Board) {
        return reject("Bench rearrange is not implemented in stage one");
    }

    const bool ok = game_->returnToBench(dragData.boardPosition, static_cast<std::size_t>(targetSlot));
    return {ok, ok ? "Unit returned to bench" : "Invalid placement"};
}

PlacementResult PlacementController::reject(QString message) const {
    return {false, std::move(message)};
}

bool PlacementController::isPlayerUnit(UnitId id) const {
    const Unit* unit = game_->unit(id);
    return unit != nullptr && unit->owner() == Owner::PlayerCtrl;
}

}  // namespace synera::gui
