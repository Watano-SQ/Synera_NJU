#pragma once

#include "AssetManager.h"
#include "BenchWidget.h"
#include "BoardWidget.h"
#include "InspectorPanel.h"
#include "PlacementController.h"
#include "core/GameState.h"

#include <QLabel>
#include <QMainWindow>

#include <optional>

namespace synera::gui {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void refreshFromState();

private:
    void initializeGame();
    void setSelectedUnit(std::optional<UnitId> unitId);
    void applyPlacementResult(const PlacementResult& result);
    QString phaseText() const;

    GameState game_;
    PlacementController placementController_;
    AssetManager assets_;
    BoardWidget* boardWidget_ = nullptr;
    BenchWidget* benchWidget_ = nullptr;
    InspectorPanel* inspectorPanel_ = nullptr;
    QLabel* playerStatusLabel_ = nullptr;
    QLabel* phaseLabel_ = nullptr;
    std::optional<UnitId> selectedUnit_;
};

}  // namespace synera::gui
