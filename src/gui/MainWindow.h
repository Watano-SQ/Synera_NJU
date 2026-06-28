#pragma once

#include "AssetManager.h"
#include "BenchWidget.h"
#include "BoardWidget.h"
#include "EquipmentPanel.h"
#include "InspectorPanel.h"
#include "PlacementController.h"
#include "ShopPanel.h"
#include "SynergyPanel.h"
#include "core/GameState.h"

#include <QAudioOutput>
#include <QLabel>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QPushButton>
#include <QTimer>

#include <optional>

namespace synera::gui {

class FlagMeterWidget;
class StatusIconWidget;

// GUI 主窗口持有 GameState，并把各个面板的用户操作接到核心规则接口上。
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void refreshFromState();

private:
    void initializeGame();
    void setSelectedUnit(std::optional<UnitId> unitId);
    void applyPlacementResult(const PlacementResult& result);
    void purchaseShopSlot(std::size_t index);
    void refreshShop();
    void upgradePopulation();
    void setSelectedItem(std::optional<ItemId> itemId);
    bool tryEquipSelectedItem(UnitId unitId);
    void saveGame();
    void loadGame();
    void startCombat();
    void advanceCombat();
    void resolveRound();
    void startBackgroundMusic();
    QString phaseText() const;

    GameState game_;
    PlacementController placementController_;
    AssetManager assets_;
    BoardWidget* boardWidget_ = nullptr;
    BenchWidget* benchWidget_ = nullptr;
    InspectorPanel* inspectorPanel_ = nullptr;
    ShopPanel* shopPanel_ = nullptr;
    EquipmentPanel* equipmentPanel_ = nullptr;
    SynergyPanel* synergyPanel_ = nullptr;
    StatusIconWidget* brainIconWidget_ = nullptr;
    QLabel* hpValueLabel_ = nullptr;
    FlagMeterWidget* flagMeterWidget_ = nullptr;
    QLabel* roundValueLabel_ = nullptr;
    QLabel* levelValueLabel_ = nullptr;
    QLabel* populationValueLabel_ = nullptr;
    QLabel* phaseLabel_ = nullptr;
    QPushButton* upgradeButton_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QPushButton* loadButton_ = nullptr;
    QPushButton* startCombatButton_ = nullptr;
    QPushButton* resolveButton_ = nullptr;
    QTimer* combatTimer_ = nullptr;
    QMediaPlayer* backgroundMusic_ = nullptr;
    QAudioOutput* backgroundAudio_ = nullptr;
    std::optional<UnitId> selectedUnit_;
    std::optional<ItemId> selectedItem_;
};

}  // namespace synera::gui
