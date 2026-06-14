#include "MainWindow.h"

#include "core/Unit.h"

#include <QHBoxLayout>
#include <QStatusBar>
#include <QVBoxLayout>

#include <memory>

namespace synera::gui {
namespace {

std::unique_ptr<Unit> makePlayerUnit(const std::string& name,
                                     std::vector<std::string> traits,
                                     const std::string& visualKey) {
    return std::make_unique<BasicUnit>(name, Owner::PlayerCtrl, 320, 35, 1, 60, std::move(traits), visualKey);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      placementController_(&game_),
      assets_(QString::fromUtf8(SYNERA_PROJECT_ROOT)) {
    initializeGame();

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* statusRow = new QHBoxLayout();
    playerStatusLabel_ = new QLabel(this);
    phaseLabel_ = new QLabel(this);
    statusRow->addWidget(playerStatusLabel_);
    statusRow->addStretch();
    statusRow->addWidget(phaseLabel_);
    root->addLayout(statusRow);

    auto* contentRow = new QHBoxLayout();
    auto* leftColumn = new QVBoxLayout();
    boardWidget_ = new BoardWidget(&game_, &assets_, this);
    benchWidget_ = new BenchWidget(&game_, &assets_, this);
    inspectorPanel_ = new InspectorPanel(&game_, this);

    leftColumn->addWidget(boardWidget_, 0, Qt::AlignLeft);
    leftColumn->addWidget(benchWidget_, 0, Qt::AlignLeft);
    contentRow->addLayout(leftColumn);
    contentRow->addWidget(inspectorPanel_, 1);
    root->addLayout(contentRow);
    setCentralWidget(central);

    boardWidget_->setUnitSelectedCallback([this](std::optional<UnitId> id) { setSelectedUnit(id); });
    benchWidget_->setUnitSelectedCallback([this](std::optional<UnitId> id) { setSelectedUnit(id); });
    boardWidget_->setBoardDropCallback([this](const UnitDragData& data, Position target) {
        applyPlacementResult(placementController_.dropOnBoard(data, target));
    });
    benchWidget_->setBenchDropCallback([this](const UnitDragData& data, int targetSlot) {
        applyPlacementResult(placementController_.dropOnBench(data, targetSlot));
    });

    setWindowTitle("Synera - Stage 1 GUI");
    resize(900, 760);
    refreshFromState();
    statusBar()->showMessage("Ready");
}

void MainWindow::refreshFromState() {
    playerStatusLabel_->setText(
        QString("HP %1  Gold %2  Level %3  Cap %4  Round %5")
            .arg(game_.player().hp())
            .arg(game_.player().gold())
            .arg(game_.player().level())
            .arg(game_.player().unitCap())
            .arg(game_.player().currentRound()));
    phaseLabel_->setText("Phase: " + phaseText());

    const bool canDrag = placementController_.phase() == GuiPhase::Prep;
    boardWidget_->setDragEnabled(canDrag);
    benchWidget_->setDragEnabled(canDrag);
    boardWidget_->setSelectedUnit(selectedUnit_);
    benchWidget_->setSelectedUnit(selectedUnit_);
    inspectorPanel_->setSelectedUnit(selectedUnit_);
    boardWidget_->refreshFromState();
    benchWidget_->refreshFromState();
    inspectorPanel_->refreshFromState();
}

void MainWindow::initializeGame() {
    game_.player().setGold(5);
    game_.player().setLevel(1);
    game_.player().setUnitCap(3);

    const UnitId vanguard =
        game_.addUnitToBench(makePlayerUnit("Aster Vanguard", {"Warrior", "Human"}, "player_vanguard"));
    game_.addUnitToBench(makePlayerUnit("Mira Spark", {"Mage", "Human"}, "player_mage"));
    game_.addUnitToBench(makePlayerUnit("Iris Guard", {"Guardian", "Human"}, "player_guard"));
    game_.deployFromBench(0, Position{7, 3}, PlacementPolicy::Reject);
    selectedUnit_ = vanguard;

    game_.generateEnemiesForRound(1);
}

void MainWindow::setSelectedUnit(std::optional<UnitId> unitId) {
    selectedUnit_ = unitId;
    refreshFromState();
}

void MainWindow::applyPlacementResult(const PlacementResult& result) {
    statusBar()->showMessage(result.message, 2500);
    refreshFromState();
}

QString MainWindow::phaseText() const {
    switch (placementController_.phase()) {
        case GuiPhase::Prep:
            return "Prep";
        case GuiPhase::Combat:
            return "Combat";
        case GuiPhase::Resolve:
            return "Resolve";
    }
    return "Unknown";
}

}  // namespace synera::gui
