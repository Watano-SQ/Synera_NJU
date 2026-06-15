#include "EquipmentPanel.h"

#include "core/Catalog.h"

#include <QLayoutItem>

namespace synera::gui {

EquipmentPanel::EquipmentPanel(const GameState* game, QWidget* parent) : QWidget(parent), game_(game) {
    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("Equipment", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    emptyLabel_ = new QLabel("-", this);
    itemLayout_ = new QVBoxLayout();
    root->addLayout(itemLayout_);
    root->addWidget(emptyLabel_);
    refreshFromState();
}

void EquipmentPanel::setSelectedItem(std::optional<ItemId> itemId) {
    selectedItem_ = itemId;
    refreshFromState();
}

void EquipmentPanel::setItemSelectedCallback(ItemSelectedCallback callback) {
    itemSelectedCallback_ = std::move(callback);
}

void EquipmentPanel::refreshFromState() {
    while (QLayoutItem* item = itemLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const auto inventory = game_->equipmentInventory();
    emptyLabel_->setVisible(inventory.empty());
    for (const ItemInstance& instance : inventory) {
        const ItemDefinition* definition = findItemDefinition(instance.itemDefId);
        const QString label = definition != nullptr ? QString::fromStdString(definition->name)
                                                    : QString::fromStdString(instance.itemDefId);
        auto* button = new QPushButton(label + (selectedItem_ == instance.itemId ? " *" : ""), this);
        button->setEnabled(game_->phase() == GamePhase::Prep);
        connect(button, &QPushButton::clicked, this, [this, itemId = instance.itemId]() {
            selectedItem_ = itemId;
            if (itemSelectedCallback_) {
                itemSelectedCallback_(selectedItem_);
            }
            refreshFromState();
        });
        itemLayout_->addWidget(button);
    }
}

}  // namespace synera::gui
