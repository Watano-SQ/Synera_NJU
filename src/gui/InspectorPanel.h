#pragma once

#include "core/GameState.h"

#include <QLabel>
#include <QWidget>

#include <optional>

namespace synera::gui {

class AssetManager;
class InspectorIconWidget;

class InspectorPanel : public QWidget {
public:
    explicit InspectorPanel(const GameState* game, AssetManager* assets, QWidget* parent = nullptr);

    void setSelectedUnit(std::optional<UnitId> unitId);
    void refreshFromState();

private:
    QString traitsText(const Unit& unit) const;
    QString itemText(const Unit& unit) const;

    const GameState* game_;
    AssetManager* assets_;
    std::optional<UnitId> selectedUnit_;
    InspectorIconWidget* unitIcon_;
    InspectorIconWidget* itemIcon_;
    QLabel* nameValue_;
    QLabel* starValue_;
    QLabel* equipmentValue_;
    QLabel* archetypeValue_;
    QLabel* ownerValue_;
    QLabel* stateValue_;
    QLabel* hpValue_;
    QLabel* atkValue_;
    QLabel* rangeValue_;
    QLabel* manaValue_;
    QLabel* baseStatsValue_;
    QLabel* effectiveStatsValue_;
    QLabel* traitsValue_;
    QLabel* placementValue_;
    QLabel* visualKeyValue_;
};

}  // namespace synera::gui
