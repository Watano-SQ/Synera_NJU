#include "BenchWidget.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace synera::gui {
namespace {

constexpr int kSlotSize = 64;
constexpr int kGap = 8;
constexpr int kMargin = 12;

QString shortLabel(const Unit& unit, UnitId id) {
    const QString prefix = unit.owner() == Owner::PlayerCtrl ? "P" : "E";
    return prefix + QString::number(id);
}

}  // namespace

BenchWidget::BenchWidget(const GameState* game, AssetManager* assets, QWidget* parent)
    : QWidget(parent), game_(game), assets_(assets) {
    setAcceptDrops(true);
    setMinimumSize(sizeHint());
}

QSize BenchWidget::sizeHint() const {
    const int slotCount = static_cast<int>(game_->bench().capacity());
    return QSize(kMargin * 2 + slotCount * kSlotSize + (slotCount - 1) * kGap, kMargin * 2 + kSlotSize);
}

void BenchWidget::setSelectedUnit(std::optional<UnitId> unitId) {
    selectedUnit_ = unitId;
    update();
}

void BenchWidget::setDragEnabled(bool enabled) {
    dragEnabled_ = enabled;
}

void BenchWidget::setUnitSelectedCallback(UnitSelectedCallback callback) {
    unitSelectedCallback_ = std::move(callback);
}

void BenchWidget::setBenchDropCallback(BenchDropCallback callback) {
    benchDropCallback_ = std::move(callback);
}

void BenchWidget::refreshFromState() {
    updateGeometry();
    update();
}

void BenchWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#f7f8fb"));

    for (int slot = 0; slot < static_cast<int>(game_->bench().capacity()); ++slot) {
        const QRect rect = slotRect(slot);
        painter.fillRect(rect, QColor("#fff8df"));
        painter.setPen(QPen(QColor("#9c8c5c"), 1));
        painter.drawRect(rect);

        const auto id = game_->bench().occupant(slot);
        if (id.has_value()) {
            drawUnit(painter, rect.adjusted(5, 5, -5, -5), *id);
        }
    }
}

void BenchWidget::mousePressEvent(QMouseEvent* event) {
    pressedSlot_.reset();
    if (event->button() != Qt::LeftButton) {
        return;
    }

    const auto slot = slotAt(event->pos());
    if (!slot.has_value()) {
        if (unitSelectedCallback_) {
            unitSelectedCallback_(std::nullopt);
        }
        return;
    }

    const auto id = game_->bench().occupant(*slot);
    if (!id.has_value()) {
        if (unitSelectedCallback_) {
            unitSelectedCallback_(std::nullopt);
        }
        return;
    }

    if (unitSelectedCallback_) {
        unitSelectedCallback_(*id);
    }

    if (canStartDrag(*id)) {
        pressedSlot_ = *slot;
        dragStartPosition_ = event->pos();
    }
}

void BenchWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!pressedSlot_.has_value()) {
        return;
    }
    if ((event->pos() - dragStartPosition_).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    const auto id = game_->bench().occupant(*pressedSlot_);
    if (!id.has_value() || !canStartDrag(*id)) {
        pressedSlot_.reset();
        return;
    }

    UnitDragData data;
    data.unitId = *id;
    data.sourceType = DragSourceType::Bench;
    data.benchSlot = *pressedSlot_;

    auto* drag = new QDrag(this);
    auto* mime = new QMimeData();
    mime->setData(kUnitDragMimeType, encodeDragData(data));
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
    pressedSlot_.reset();
}

void BenchWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(kUnitDragMimeType)) {
        event->acceptProposedAction();
    }
}

void BenchWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat(kUnitDragMimeType) && slotAt(event->position().toPoint()).has_value()) {
        event->acceptProposedAction();
    }
}

void BenchWidget::dropEvent(QDropEvent* event) {
    UnitDragData data;
    const auto targetSlot = slotAt(event->position().toPoint());
    if (!targetSlot.has_value() || !decodeDragData(event->mimeData()->data(kUnitDragMimeType), data)) {
        event->ignore();
        return;
    }

    if (benchDropCallback_) {
        benchDropCallback_(data, *targetSlot);
    }
    event->acceptProposedAction();
}

std::optional<int> BenchWidget::slotAt(const QPoint& point) const {
    for (int slot = 0; slot < static_cast<int>(game_->bench().capacity()); ++slot) {
        if (slotRect(slot).contains(point)) {
            return slot;
        }
    }
    return std::nullopt;
}

QRect BenchWidget::slotRect(int slot) const {
    return QRect(kMargin + slot * (kSlotSize + kGap), kMargin, kSlotSize, kSlotSize);
}

void BenchWidget::drawUnit(QPainter& painter, const QRect& rect, UnitId id) const {
    const Unit* unit = game_->unit(id);
    if (unit == nullptr) {
        return;
    }

    const QPixmap* pixmap = assets_ != nullptr ? assets_->pixmapFor(unit->visualKey()) : nullptr;
    if (pixmap != nullptr) {
        painter.drawPixmap(rect.adjusted(2, 2, -2, -12), *pixmap);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#2f6fed"));
        painter.drawRoundedRect(rect.adjusted(2, 2, -2, -12), 6, 6);
        painter.setPen(Qt::white);
        painter.drawText(rect.adjusted(4, 4, -4, -16), Qt::AlignCenter, shortLabel(*unit, id));
    }

    const int hpWidth = std::max(1, rect.width() * unit->hp() / std::max(1, unit->maxHp()));
    const int manaWidth = std::max(1, rect.width() * unit->mana() / std::max(1, unit->maxMana()));
    const QRect hpBack(rect.left(), rect.bottom() - 10, rect.width(), 4);
    const QRect manaBack(rect.left(), rect.bottom() - 5, rect.width(), 4);
    painter.fillRect(hpBack, QColor("#d9dde5"));
    painter.fillRect(QRect(hpBack.left(), hpBack.top(), hpWidth, hpBack.height()), QColor("#30a46c"));
    painter.fillRect(manaBack, QColor("#d9dde5"));
    painter.fillRect(QRect(manaBack.left(), manaBack.top(), manaWidth, manaBack.height()), QColor("#2f81f7"));

    if (selectedUnit_.has_value() && *selectedUnit_ == id) {
        painter.setPen(QPen(QColor("#ffb000"), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
    }
}

bool BenchWidget::canStartDrag(UnitId id) const {
    if (!dragEnabled_) {
        return false;
    }
    const Unit* unit = game_->unit(id);
    return unit != nullptr && unit->owner() == Owner::PlayerCtrl;
}

}  // namespace synera::gui
