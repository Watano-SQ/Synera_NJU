#pragma once

#include "core/GameState.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <optional>

namespace synera::gui {

class EquipmentPanel : public QWidget {
public:
    using ItemSelectedCallback = std::function<void(std::optional<ItemId>)>;

    explicit EquipmentPanel(const GameState* game, QWidget* parent = nullptr);

    void setSelectedItem(std::optional<ItemId> itemId);
    void setItemSelectedCallback(ItemSelectedCallback callback);
    void refreshFromState();

private:
    const GameState* game_;
    std::optional<ItemId> selectedItem_;
    QVBoxLayout* itemLayout_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    ItemSelectedCallback itemSelectedCallback_;
};

}  // namespace synera::gui
