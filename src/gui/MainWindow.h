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

    // 统一刷新入口：任何会改变 GameState 的操作完成后都调用它，让所有面板重新从规则层读取数据。
    // 这样可以避免某个面板偷偷缓存旧金币、旧羁绊或旧选中单位。
    void refreshFromState();

private:
    // 初始化一局新游戏，同时建立第一轮商店、初始资源和默认界面状态。
    void initializeGame();
    // selectedUnit_ 是 GUI 选择状态，不属于存档规则；读档或战斗结束后可以安全重置。
    void setSelectedUnit(std::optional<UnitId> unitId);
    // 将 PlacementController 的结果写到状态栏，并在成功时刷新棋盘、备战区和属性面板。
    void applyPlacementResult(const PlacementResult& result);
    // 以下函数是按钮/面板回调的薄封装：它们不直接改 Board/Bench，只调用 GameState 的公开命令。
    void purchaseShopSlot(std::size_t index);
    void refreshShop();
    void upgradePopulation();
    // selectedItem_ 表示“准备穿戴的库存装备”；真正穿戴在 tryEquipSelectedItem 中完成。
    void setSelectedItem(std::optional<ItemId> itemId);
    bool tryEquipSelectedItem(UnitId unitId);
    // 存读档使用 GameState 的文本格式；MainWindow 只负责文件选择和结果提示。
    void saveGame();
    void loadGame();
    // 战斗推进由 QTimer 周期性调用 advanceCombat，直到 GameState 进入 Resolve。
    void startCombat();
    void advanceCombat();
    void resolveRound();
    // 背景音乐是纯表现层资源，失败不影响核心规则运行。
    void startBackgroundMusic();
    QString phaseText() const;

    // MainWindow 拥有整局游戏状态；其它 GUI 控件都只拿 const GameState* 或通过控制器发命令。
    GameState game_;
    PlacementController placementController_;
    AssetManager assets_;
    // 以下裸指针由 Qt 父子对象树管理生命周期，MainWindow 析构时会随父控件一起释放。
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
    // 选中状态只服务 GUI 展示和装备交互，不参与 GameState 的核心规则。
    std::optional<UnitId> selectedUnit_;
    std::optional<ItemId> selectedItem_;
};

}  // namespace synera::gui
