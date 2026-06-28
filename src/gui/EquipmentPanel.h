#pragma once

#include "core/GameState.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <optional>

namespace synera::gui {

class AssetManager;
class EquipmentTrayWidget;

// 装备面板展示库存装备，并让玩家先选装备再点单位穿戴。
class EquipmentPanel : public QWidget {
public:
    // 选择装备只是 UI 状态；真正穿戴需要玩家随后点击一个单位，并由 MainWindow 调用 GameState::equipItem。
    using ItemSelectedCallback = std::function<void(std::optional<ItemId>)>;

    explicit EquipmentPanel(const GameState* game, AssetManager* assets, QWidget* parent = nullptr);

    void setSelectedItem(std::optional<ItemId> itemId);
    void setItemSelectedCallback(ItemSelectedCallback callback);
    void refreshFromState();

private:
    // EquipmentPanel 不拥有游戏状态或资源缓存，只读取库存并显示可选择的装备。
    const GameState* game_;
    AssetManager* assets_;
    // selectedItem_ 用来高亮当前准备穿戴的装备，同时同步给 MainWindow。
    std::optional<ItemId> selectedItem_;
    EquipmentTrayWidget* trayWidget_ = nullptr;
    QVBoxLayout* itemLayout_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    ItemSelectedCallback itemSelectedCallback_;
};

}  // namespace synera::gui
