#include "EquipmentPanel.h"

#include "AssetManager.h"
#include "core/Catalog.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLayoutItem>
#include <QMouseEvent>
#include <QPainter>
#include <QSize>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace synera::gui {
namespace {

QRect aspectFitRect(const QRect& target, const QSize& sourceSize) {
    if (sourceSize.isEmpty() || target.isEmpty()) {
        return QRect();
    }
    const QSize scaled = sourceSize.scaled(target.size(), Qt::KeepAspectRatio);
    return QRect(QPoint(target.center().x() - scaled.width() / 2,
                       target.center().y() - scaled.height() / 2),
                 scaled);
}

void drawPixmapAspectFit(QPainter& painter, const QRect& target, const QPixmap& pixmap) {
    if (pixmap.isNull() || target.isEmpty()) {
        return;
    }
    painter.drawPixmap(aspectFitRect(target, pixmap.size()), pixmap);
}

}  // namespace

class EquipmentTrayWidget : public QWidget {
public:
    EquipmentTrayWidget(const GameState* game, AssetManager* assets, QWidget* parent = nullptr)
        : QWidget(parent), game_(game), assets_(assets) {
        setObjectName("equipmentTray");
        setFixedSize(sizeHint());
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setMouseTracking(true);
    }

    QSize sizeHint() const override {
        return QSize(500, 96);
    }

    QRect trayRectForTesting() const {
        return trayRect();
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

        painter.fillRect(rect(), QColor("#2f251d"));
        const QRect tray = trayRect();
        const QPixmap* background = assets_ != nullptr ? assets_->pixmapFor("ui/zombieline") : nullptr;
        if (background != nullptr) {
            drawPixmapAspectFit(painter, tray, *background);
        } else {
            painter.setPen(QPen(QColor("#211811"), 2));
            painter.setBrush(QColor("#594233"));
            painter.drawRoundedRect(tray, 8, 8);
        }

        constexpr int kVisibleSlots = 4;
        for (int i = 0; i < kVisibleSlots; ++i) {
            const QRect slot = slotRect(i);
            painter.setPen(QPen(QColor(24, 18, 12, 200), 2));
            painter.setBrush(QColor(250, 231, 167, 72));
            painter.drawRoundedRect(slot, 7, 7);
        }

        const int visibleItems = std::min(kVisibleSlots, static_cast<int>(inventory_.size()));
        for (int i = 0; i < visibleItems; ++i) {
            const ItemInstance& instance = inventory_[static_cast<std::size_t>(i)];
            const ItemDefinition* definition = findItemDefinition(instance.itemDefId);
            const QRect slot = slotRect(i);
            if (definition != nullptr && assets_ != nullptr) {
                if (const QPixmap* icon = assets_->pixmapFor(definition->visualKey)) {
                    drawPixmapAspectFit(painter, slot.adjusted(6, 6, -6, -6), *icon);
                }
            }
            if (selectedItem_ == instance.itemId) {
                painter.setPen(QPen(QColor("#ffd34e"), 3));
                painter.setBrush(QColor(255, 211, 78, 55));
                painter.drawRoundedRect(slot.adjusted(1, 1, -1, -1), 7, 7);
            }
        }

        if (game_->phase() != GamePhase::Prep) {
            painter.fillRect(tray, QColor(0, 0, 0, 60));
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton || game_->phase() != GamePhase::Prep) {
            return;
        }

        constexpr int kVisibleSlots = 4;
        const int visibleItems = std::min(kVisibleSlots, static_cast<int>(inventory_.size()));
        for (int i = 0; i < visibleItems; ++i) {
            if (slotRect(i).contains(event->pos())) {
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
    QRect trayRect() const {
        const QPixmap* background = assets_ != nullptr ? assets_->pixmapFor("ui/zombieline") : nullptr;
        const QSize sourceSize = background != nullptr && !background->isNull() ? background->size() : QSize(190, 40);
        const int maxWidth = std::max(1, width() - 16);
        const int maxHeight = std::max(1, height() - 8);
        const double ratio = static_cast<double>(sourceSize.width()) / std::max(1, sourceSize.height());
        int trayHeight = std::min(92, maxHeight);
        int trayWidth = static_cast<int>(std::round(trayHeight * ratio));
        if (trayWidth > maxWidth) {
            trayWidth = maxWidth;
            trayHeight = static_cast<int>(std::round(trayWidth / ratio));
        }
        return QRect((width() - trayWidth) / 2, (height() - trayHeight) / 2, trayWidth, trayHeight);
    }

    QRect slotRect(int index) const {
        const QRect tray = trayRect();
        const int slotSize = std::clamp((tray.width() - 48 - 3 * 8) / 4, 44, 52);
        const int rawGap = (tray.width() - 48 - 4 * slotSize) / 3;
        const int gap = std::clamp(rawGap, 8, 34);
        const int totalWidth = 4 * slotSize + 3 * gap;
        const int left = tray.left() + (tray.width() - totalWidth) / 2 + index * (slotSize + gap);
        const int top = tray.center().y() - slotSize / 2;
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
    setFixedHeight(130);
    setMinimumWidth(500);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 4, 8, 6);
    root->setSpacing(4);

    auto* title = new QLabel("Equipment", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setFixedHeight(18);
    root->addWidget(title);

    trayWidget_ = new EquipmentTrayWidget(game_, assets_, this);
    root->addWidget(trayWidget_, 0, Qt::AlignHCenter | Qt::AlignTop);

    emptyLabel_ = new QLabel("-", this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    itemLayout_ = new QVBoxLayout();
    itemLayout_->setContentsMargins(0, 0, 0, 0);
    itemLayout_->setSpacing(4);
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
        button->setFixedHeight(28);
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
