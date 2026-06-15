#include "EquipmentPanel.h"

#include "AssetManager.h"
#include "core/Catalog.h"

#include <QIcon>
#include <QLayoutItem>
#include <QMouseEvent>
#include <QPainter>
#include <QSize>

#include <algorithm>
#include <utility>
#include <vector>

namespace synera::gui {

class EquipmentTrayWidget : public QWidget {
public:
    EquipmentTrayWidget(const GameState* game, AssetManager* assets, QWidget* parent = nullptr)
        : QWidget(parent), game_(game), assets_(assets) {
        setMinimumSize(sizeHint());
        setMouseTracking(true);
    }

    QSize sizeHint() const override {
        return QSize(320, 92);
    }

    void setSelectedItem(std::optional<ItemId> itemId) {
        selectedItem_ = itemId;
        update();
    }

    void setItemSelectedCallback(EquipmentPanel::ItemSelectedCallback callback) {
        itemSelectedCallback_ = std::move(callback);
    }

    void refreshFromState() {
        inventory_ = game_->equipmentInventory();
        updateGeometry();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QPixmap* background = assets_ != nullptr ? assets_->pixmapFor("ui/zombieline") : nullptr;
        if (background != nullptr) {
            painter.drawPixmap(rect(), *background);
        } else {
            painter.fillRect(rect(), QColor("#47372b"));
        }

        const int slotCount = std::max(4, static_cast<int>(inventory_.size()));
        for (int i = 0; i < slotCount; ++i) {
            const QRect slot = slotRect(i, slotCount);
            painter.setPen(QPen(QColor(24, 18, 12, 180), 2));
            painter.setBrush(QColor(250, 231, 167, 90));
            painter.drawRoundedRect(slot, 6, 6);
        }

        for (int i = 0; i < static_cast<int>(inventory_.size()); ++i) {
            const ItemInstance& instance = inventory_[static_cast<std::size_t>(i)];
            const ItemDefinition* definition = findItemDefinition(instance.itemDefId);
            const QRect slot = slotRect(i, slotCount);
            if (definition != nullptr && assets_ != nullptr) {
                if (const QPixmap* icon = assets_->pixmapFor(definition->visualKey)) {
                    painter.drawPixmap(slot.adjusted(7, 7, -7, -7), *icon);
                }
            }
            if (selectedItem_ == instance.itemId) {
                painter.setPen(QPen(QColor("#ffd34e"), 3));
                painter.setBrush(QColor(255, 211, 78, 55));
                painter.drawRoundedRect(slot.adjusted(1, 1, -1, -1), 6, 6);
            }
        }

        if (game_->phase() != GamePhase::Prep) {
            painter.fillRect(rect(), QColor(0, 0, 0, 60));
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton || game_->phase() != GamePhase::Prep) {
            return;
        }

        const int slotCount = std::max(4, static_cast<int>(inventory_.size()));
        for (int i = 0; i < static_cast<int>(inventory_.size()); ++i) {
            if (slotRect(i, slotCount).contains(event->pos())) {
                selectedItem_ = inventory_[static_cast<std::size_t>(i)].itemId;
                if (itemSelectedCallback_) {
                    itemSelectedCallback_(selectedItem_);
                }
                update();
                return;
            }
        }
    }

private:
    QRect slotRect(int index, int slotCount) const {
        constexpr int gap = 8;
        const int availableWidth = std::max(1, width() - 28);
        const int rawSize = (availableWidth - gap * (slotCount - 1)) / std::max(1, slotCount);
        const int slotSize = std::clamp(rawSize, 30, 48);
        const int totalWidth = slotCount * slotSize + (slotCount - 1) * gap;
        const int left = (width() - totalWidth) / 2 + index * (slotSize + gap);
        const int top = (height() - slotSize) / 2;
        return QRect(left, top, slotSize, slotSize);
    }

    const GameState* game_;
    AssetManager* assets_;
    std::optional<ItemId> selectedItem_;
    std::vector<ItemInstance> inventory_;
    EquipmentPanel::ItemSelectedCallback itemSelectedCallback_;
};

EquipmentPanel::EquipmentPanel(const GameState* game, AssetManager* assets, QWidget* parent)
    : QWidget(parent), game_(game), assets_(assets) {
    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("装备栏", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    trayWidget_ = new EquipmentTrayWidget(game_, assets_, this);
    root->addWidget(trayWidget_);

    emptyLabel_ = new QLabel("-", this);
    itemLayout_ = new QVBoxLayout();
    root->addLayout(itemLayout_);
    root->addWidget(emptyLabel_);
    refreshFromState();
}

void EquipmentPanel::setSelectedItem(std::optional<ItemId> itemId) {
    selectedItem_ = itemId;
    trayWidget_->setSelectedItem(itemId);
    refreshFromState();
}

void EquipmentPanel::setItemSelectedCallback(ItemSelectedCallback callback) {
    itemSelectedCallback_ = std::move(callback);
    trayWidget_->setItemSelectedCallback(itemSelectedCallback_);
}

void EquipmentPanel::refreshFromState() {
    while (QLayoutItem* item = itemLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const auto inventory = game_->equipmentInventory();
    const bool useTray = assets_ != nullptr && assets_->pixmapFor("ui/zombieline") != nullptr;
    trayWidget_->setVisible(useTray);
    emptyLabel_->setVisible(!useTray && inventory.empty());

    if (useTray) {
        trayWidget_->setSelectedItem(selectedItem_);
        trayWidget_->refreshFromState();
        return;
    }

    for (const ItemInstance& instance : inventory) {
        const ItemDefinition* definition = findItemDefinition(instance.itemDefId);
        const QString label = definition != nullptr ? QString::fromStdString(definition->name)
                                                    : QString::fromStdString(instance.itemDefId);
        auto* button = new QPushButton(label + (selectedItem_ == instance.itemId ? " *" : ""), this);
        if (definition != nullptr && assets_ != nullptr) {
            const QPixmap* pixmap = assets_->pixmapFor(definition->visualKey);
            if (pixmap != nullptr) {
                button->setIcon(QIcon(*pixmap));
                button->setIconSize(QSize(28, 28));
            }
        }
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
