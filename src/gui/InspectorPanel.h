#pragma once

#include "core/GameState.h"

#include <QLabel>
#include <QWidget>

#include <optional>

namespace synera::gui {

class InspectorPanel : public QWidget {
public:
    explicit InspectorPanel(const GameState* game, QWidget* parent = nullptr);

    void setSelectedUnit(std::optional<UnitId> unitId);
    void refreshFromState();

private:
    QString traitsText(const Unit& unit) const;

    const GameState* game_;
    std::optional<UnitId> selectedUnit_;
    QLabel* nameValue_;
    QLabel* ownerValue_;
    QLabel* hpValue_;
    QLabel* atkValue_;
    QLabel* rangeValue_;
    QLabel* manaValue_;
    QLabel* traitsValue_;
    QLabel* placementValue_;
    QLabel* visualKeyValue_;
};

}  // namespace synera::gui
