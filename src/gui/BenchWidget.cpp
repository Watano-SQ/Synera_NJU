#include "BenchWidget.h"

#include "PaintUtils.h"
#include "core/Catalog.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>

#include <algorithm>

namespace synera::gui {
namespace {

constexpr int kBenchWidth = 512;
constexpr int kBenchHeight = 96;
constexpr int kSlotWidth = 58;
constexpr int kSlotHeight = 64;
constexpr int kSlotGap = 5;
constexpr int kPaddingLeft = 6;
constexpr int kPaddingTop = 14;

QString shortLabel(const Unit& unit, UnitId id) {
    const QString prefix = unit.owner() == Owner::PlayerCtrl ? "P" : "E";
    return prefix + QString::number(id);
}

}  // namespace

BenchWidget::BenchWidget(const GameState* game, AssetManager* assets, QWidget* parent)
    : QWidget(parent), game_(game), assets_(assets) {
    setAcceptDrops(true);
    setMouseTracking(true);
    setFixedSize(sizeHint());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize BenchWidget::sizeHint() const {
    return QSize(kBenchWidth, kBenchHeight);
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
    painter.fillRect(rect(), QColor("#315c2c"));
    painter.setPen(QPen(QColor("#203d1e"), 2));
    painter.setBrush(QColor(72, 119, 53, 190));
    painter.drawRoundedRect(rect().adjusted(3, 8, -3, -8), 7, 7);
    painter.setPen(QPen(QColor(186, 220, 129, 70), 1));
    for (int x = 12; x < width(); x += 24) {
        painter.drawLine(x, 14, x + 12, height() - 15);
    }

    for (int slot = 0; slot < static_cast<int>(game_->bench().capacity()); ++slot) {
        drawCellBase(painter, slotRect(slot));
    }

    for (int slot = 0; slot < static_cast<int>(game_->bench().capacity()); ++slot) {
        drawGrid(painter, slotRect(slot));
    }

    for (int slot = 0; slot < static_cast<int>(game_->bench().capacity()); ++slot) {
        const auto id = game_->bench().occupant(slot);
        if (id.has_value()) {
            drawUnit(painter, slotRect(slot), *id);
        }
    }

    for (int slot = 0; slot < static_cast<int>(game_->bench().capacity()); ++slot) {
        const auto id = game_->bench().occupant(slot);
        if (id.has_value()) {
            drawSelectionOverlay(painter, slotRect(slot).adjusted(6, 4, -6, -12), *id);
        }
    }

    for (int slot = 0; slot < static_cast<int>(game_->bench().capacity()); ++slot) {
        drawHoverOverlay(painter, slotRect(slot), slot);
    }

    for (int slot = 0; slot < static_cast<int>(game_->bench().capacity()); ++slot) {
        drawDropOverlay(painter, slotRect(slot), slot);
    }
}

void BenchWidget::mousePressEvent(QMouseEvent* event) {
    pressedSlot_.reset();
    dropTargetSlot_.reset();
    if (event->button() != Qt::LeftButton) {
        update();
        return;
    }

    const auto slot = slotAt(event->pos());
    hoverSlot_ = slot;
    if (!slot.has_value()) {
        if (unitSelectedCallback_) {
            unitSelectedCallback_(std::nullopt);
        }
        update();
        return;
    }

    const auto id = game_->bench().occupant(*slot);
    if (!id.has_value()) {
        if (unitSelectedCallback_) {
            unitSelectedCallback_(std::nullopt);
        }
        update();
        return;
    }

    if (unitSelectedCallback_) {
        unitSelectedCallback_(*id);
    }

    if (canStartDrag(*id)) {
        pressedSlot_ = *slot;
        dragStartPosition_ = event->pos();
    }
    update();
}

void BenchWidget::mouseMoveEvent(QMouseEvent* event) {
    const auto currentSlot = slotAt(event->pos());
    if (hoverSlot_ != currentSlot) {
        hoverSlot_ = currentSlot;
        update();
    }

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
    dropTargetSlot_.reset();
    update();
}

void BenchWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(kUnitDragMimeType)) {
        event->acceptProposedAction();
    }
}

void BenchWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat(kUnitDragMimeType) && slotAt(event->position().toPoint()).has_value()) {
        dropTargetSlot_ = slotAt(event->position().toPoint());
        update();
        event->acceptProposedAction();
        return;
    }
    dropTargetSlot_.reset();
    update();
}

void BenchWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    dropTargetSlot_.reset();
    update();
    event->accept();
}

void BenchWidget::dropEvent(QDropEvent* event) {
    UnitDragData data;
    const auto targetSlot = slotAt(event->position().toPoint());
    if (!targetSlot.has_value() || !decodeDragData(event->mimeData()->data(kUnitDragMimeType), data)) {
        dropTargetSlot_.reset();
        update();
        event->ignore();
        return;
    }

    if (benchDropCallback_) {
        benchDropCallback_(data, *targetSlot);
    }
    dropTargetSlot_.reset();
    update();
    event->acceptProposedAction();
}

void BenchWidget::leaveEvent(QEvent* event) {
    hoverSlot_.reset();
    update();
    QWidget::leaveEvent(event);
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
    return QRect(kPaddingLeft + slot * (kSlotWidth + kSlotGap), kPaddingTop, kSlotWidth, kSlotHeight);
}

void BenchWidget::drawCellBase(QPainter& painter, const QRect& rect) const {
    painter.save();
    const QPixmap* frame = assets_ != nullptr ? assets_->pixmapFor("ui/button") : nullptr;
    if (frame != nullptr) {
        drawPixmapAspectFit(painter, rect, *frame);
        painter.fillRect(rect.adjusted(7, 8, -7, -8), QColor(38, 83, 39, 118));
    } else {
        painter.setPen(QPen(QColor("#213e20"), 2));
        painter.setBrush(QColor("#466e32"));
        painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 6, 6);
    }
    painter.restore();
}

void BenchWidget::drawGrid(QPainter& painter, const QRect& rect) const {
    painter.save();
    painter.setPen(QPen(QColor("#3e5f28"), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect.adjusted(2, 2, -2, -2), 6, 6);
    painter.restore();
}

void BenchWidget::drawUnit(QPainter& painter, const QRect& rect, UnitId id) const {
    painter.save();
    const Unit* unit = game_->unit(id);
    if (unit == nullptr) {
        painter.restore();
        return;
    }

    const QRect unitTarget = rect.adjusted(6, 4, -6, -12);
    const QPixmap* pixmap = assets_ != nullptr ? assets_->pixmapFor(displayVisualKey(*unit)) : nullptr;
    if (pixmap != nullptr) {
        drawPixmapAspectFit(painter, unitTarget, *pixmap);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#2f6fed"));
        painter.drawRoundedRect(unitTarget, 6, 6);
        painter.setPen(Qt::white);
        painter.drawText(unitTarget, Qt::AlignCenter, shortLabel(*unit, id));
    }

    painter.restore();
    drawHpManaBars(painter, rect, *unit);
}

void BenchWidget::drawHpManaBars(QPainter& painter, const QRect& rect, const Unit& unit) const {
    painter.save();
    const int hpWidth = std::max(1, rect.width() * unit.hp() / std::max(1, unit.maxHp()));
    const int manaWidth = std::max(1, rect.width() * unit.mana() / std::max(1, unit.maxMana()));
    const QRect hpBack(rect.left(), rect.bottom() - 10, rect.width(), 4);
    const QRect manaBack(rect.left(), rect.bottom() - 5, rect.width(), 4);
    painter.fillRect(hpBack, QColor("#d9dde5"));
    painter.fillRect(QRect(hpBack.left(), hpBack.top(), hpWidth, hpBack.height()), QColor("#30a46c"));
    painter.fillRect(manaBack, QColor("#d9dde5"));
    painter.fillRect(QRect(manaBack.left(), manaBack.top(), manaWidth, manaBack.height()), QColor("#2f81f7"));
    painter.restore();
}

void BenchWidget::drawSelectionOverlay(QPainter& painter, const QRect& rect, UnitId id) const {
    painter.save();
    if (selectedUnit_.has_value() && *selectedUnit_ == id) {
        painter.setPen(QPen(QColor("#ffb000"), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
    }
    painter.restore();
}

void BenchWidget::drawHoverOverlay(QPainter& painter, const QRect& rect, int slot) const {
    painter.save();
    if (hoverSlot_.has_value() && *hoverSlot_ == slot) {
        painter.setPen(QPen(QColor(40, 40, 40, 90), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.adjusted(2, 2, -2, -2));
    }
    painter.restore();
}

void BenchWidget::drawDropOverlay(QPainter& painter, const QRect& rect, int slot) const {
    painter.save();
    if (dropTargetSlot_.has_value() && *dropTargetSlot_ == slot) {
        painter.setPen(QPen(QColor(34, 139, 230, 190), 3, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.adjusted(3, 3, -3, -3));
    }
    painter.restore();
}

bool BenchWidget::canStartDrag(UnitId id) const {
    if (!dragEnabled_) {
        return false;
    }
    const Unit* unit = game_->unit(id);
    return unit != nullptr && unit->owner() == Owner::PlayerCtrl;
}

}  // namespace synera::gui
