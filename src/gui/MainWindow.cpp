#include "MainWindow.h"

#include "core/Catalog.h"
#include "core/Unit.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QVBoxLayout>

#include <memory>

namespace synera::gui {
namespace {

std::unique_ptr<Unit> makeCatalogUnit(const std::string& definitionId) {
    const UnitDefinition* definition = findUnitDefinition(definitionId);
    return definition != nullptr ? createUnitFromDefinition(*definition, Owner::PlayerCtrl) : nullptr;
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
    upgradeButton_ = new QPushButton("升级人口", this);
    saveButton_ = new QPushButton("存档", this);
    loadButton_ = new QPushButton("读档", this);
    startCombatButton_ = new QPushButton("开始守家", this);
    resolveButton_ = new QPushButton("结算", this);
    statusRow->addWidget(playerStatusLabel_);
    statusRow->addStretch();
    statusRow->addWidget(phaseLabel_);
    statusRow->addWidget(upgradeButton_);
    statusRow->addWidget(saveButton_);
    statusRow->addWidget(loadButton_);
    statusRow->addWidget(startCombatButton_);
    statusRow->addWidget(resolveButton_);
    root->addLayout(statusRow);

    auto* contentRow = new QHBoxLayout();
    auto* leftColumn = new QVBoxLayout();
    boardWidget_ = new BoardWidget(&game_, &assets_, this);
    benchWidget_ = new BenchWidget(&game_, &assets_, this);
    inspectorPanel_ = new InspectorPanel(&game_, this);
    shopPanel_ = new ShopPanel(&game_, &assets_, this);
    equipmentPanel_ = new EquipmentPanel(&game_, &assets_, this);
    synergyPanel_ = new SynergyPanel(&game_, &assets_, this);

    leftColumn->addWidget(boardWidget_, 0, Qt::AlignLeft);
    leftColumn->addWidget(benchWidget_, 0, Qt::AlignLeft);
    contentRow->addLayout(leftColumn);
    auto* rightColumn = new QVBoxLayout();
    rightColumn->addWidget(shopPanel_);
    rightColumn->addWidget(equipmentPanel_);
    rightColumn->addWidget(synergyPanel_);
    rightColumn->addWidget(inspectorPanel_, 1);
    contentRow->addLayout(rightColumn, 1);
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
    shopPanel_->setPurchaseCallback([this](std::size_t index) { purchaseShopSlot(index); });
    shopPanel_->setRefreshCallback([this]() { refreshShop(); });
    equipmentPanel_->setItemSelectedCallback([this](std::optional<ItemId> itemId) { setSelectedItem(itemId); });
    connect(upgradeButton_, &QPushButton::clicked, this, &MainWindow::upgradePopulation);
    connect(saveButton_, &QPushButton::clicked, this, &MainWindow::saveGame);
    connect(loadButton_, &QPushButton::clicked, this, &MainWindow::loadGame);
    connect(startCombatButton_, &QPushButton::clicked, this, &MainWindow::startCombat);
    connect(resolveButton_, &QPushButton::clicked, this, &MainWindow::resolveRound);

    combatTimer_ = new QTimer(this);
    combatTimer_->setInterval(16);
    connect(combatTimer_, &QTimer::timeout, this, &MainWindow::advanceCombat);

    setWindowTitle("植物自走棋 - PvZ Auto Arena");
    resize(1180, 820);
    refreshFromState();
    statusBar()->showMessage("Ready");
}

void MainWindow::refreshFromState() {
    if (selectedUnit_.has_value() && game_.unit(*selectedUnit_) == nullptr) {
        selectedUnit_.reset();
    }

    playerStatusLabel_->setText(
        QString("脑子 %1  阳光 %2  等级 %3  人口 %4/%5  波次 %6  战局 %7")
            .arg(game_.player().hp())
            .arg(game_.player().gold())
            .arg(game_.player().level())
            .arg(game_.deployedPlayerUnitCount())
            .arg(game_.player().unitCap())
            .arg(game_.player().currentRound())
            .arg(QString::fromStdString(toString(game_.matchResult()))));
    phaseLabel_->setText("阶段: " + phaseText());

    const bool canDrag = game_.phase() == GamePhase::Prep;
    const bool canManage = game_.phase() == GamePhase::Prep;
    startCombatButton_->setEnabled(game_.phase() == GamePhase::Prep && game_.matchResult() == MatchResult::Ongoing);
    resolveButton_->setEnabled(game_.phase() == GamePhase::Resolve);
    upgradeButton_->setEnabled(canManage);
    saveButton_->setEnabled(game_.phase() != GamePhase::Combat);
    loadButton_->setEnabled(game_.phase() != GamePhase::Combat);
    boardWidget_->setDragEnabled(canDrag);
    benchWidget_->setDragEnabled(canDrag);
    boardWidget_->setSelectedUnit(selectedUnit_);
    benchWidget_->setSelectedUnit(selectedUnit_);
    inspectorPanel_->setSelectedUnit(selectedUnit_);
    boardWidget_->refreshFromState();
    benchWidget_->refreshFromState();
    inspectorPanel_->refreshFromState();
    shopPanel_->refreshFromState();
    equipmentPanel_->setSelectedItem(selectedItem_);
    equipmentPanel_->refreshFromState();
    synergyPanel_->refreshFromState();
}

void MainWindow::initializeGame() {
    game_.player().setGold(8);
    game_.player().setLevel(1);
    game_.player().setUnitCap(3);

    const UnitId starter = game_.addUnitToBench(makeCatalogUnit("peashooter"));
    game_.addUnitToBench(makeCatalogUnit("sunflower"));
    game_.addUnitToBench(makeCatalogUnit("wallnut"));
    game_.deployFromBench(0, Position{7, 3}, PlacementPolicy::Reject);
    selectedUnit_ = starter;
}

void MainWindow::setSelectedUnit(std::optional<UnitId> unitId) {
    if (unitId.has_value() && tryEquipSelectedItem(*unitId)) {
        selectedUnit_ = unitId;
        refreshFromState();
        return;
    }
    selectedUnit_ = unitId;
    refreshFromState();
}

void MainWindow::applyPlacementResult(const PlacementResult& result) {
    statusBar()->showMessage(result.message, 2500);
    refreshFromState();
}

void MainWindow::purchaseShopSlot(std::size_t index) {
    const ActionResult result = game_.purchaseShopSlot(index);
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::refreshShop() {
    const ActionResult result = game_.refreshShop();
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::upgradePopulation() {
    const ActionResult result = game_.upgradePopulation();
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::setSelectedItem(std::optional<ItemId> itemId) {
    selectedItem_ = itemId;
    refreshFromState();
}

bool MainWindow::tryEquipSelectedItem(UnitId unitId) {
    if (!selectedItem_.has_value()) {
        return false;
    }
    const ActionResult result = game_.equipItem(*selectedItem_, unitId);
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    if (result.success) {
        selectedItem_.reset();
    }
    return result.success;
}

void MainWindow::saveGame() {
    const QString path = QFileDialog::getSaveFileName(this, "Save Synera", "synera_save.txt", "Synera Save (*.txt)");
    if (path.isEmpty()) {
        return;
    }
    const ActionResult result = game_.saveToFile(path.toStdString());
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::loadGame() {
    const QString path = QFileDialog::getOpenFileName(this, "Load Synera", QString(), "Synera Save (*.txt)");
    if (path.isEmpty()) {
        return;
    }
    combatTimer_->stop();
    const ActionResult result = game_.loadFromFile(path.toStdString());
    if (result.success) {
        selectedUnit_.reset();
        selectedItem_.reset();
    }
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::startCombat() {
    const ActionResult result = game_.startCombat();
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    if (result.success && game_.phase() == GamePhase::Combat) {
        combatTimer_->start();
    }
    refreshFromState();
}

void MainWindow::advanceCombat() {
    const ActionResult result = game_.tickCombat();
    if (!result.success) {
        combatTimer_->stop();
        statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    } else if (game_.phase() == GamePhase::Resolve) {
        combatTimer_->stop();
        statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    }
    refreshFromState();
}

void MainWindow::resolveRound() {
    combatTimer_->stop();
    const ActionResult result = game_.resolveRound();
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

QString MainWindow::phaseText() const {
    switch (game_.phase()) {
        case GamePhase::Prep:
            return "准备";
        case GamePhase::Combat:
            return "守家";
        case GamePhase::Resolve:
            return "结算";
        case GamePhase::GameOver:
            return "结束";
    }
    return QString::fromStdString(toString(game_.phase()));
}

}  // namespace synera::gui
