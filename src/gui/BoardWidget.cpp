#include "BoardWidget.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace synera::gui {
namespace {

constexpr int kCellSize = 64;
constexpr int kMargin = 12;

QString shortLabel(const Unit& unit, UnitId id) {
    const QString prefix = unit.owner() == Owner::PlayerCtrl ? "P" : "E";
    return prefix + QString::number(id);
}

}  // namespace

BoardWidget::BoardWidget(const GameState* game, AssetManager* assets, QWidget* parent)
    : QWidget(parent), game_(game), assets_(assets) {
    setAcceptDrops(true);
    setMouseTracking(true);
    setMinimumSize(sizeHint());
}

QSize BoardWidget::sizeHint() const {
    return QSize(game_->board().cols() * kCellSize + kMargin * 2,
                 game_->board().rows() * kCellSize + kMargin * 2);
}

void BoardWidget::setSelectedUnit(std::optional<UnitId> unitId) {
    selectedUnit_ = unitId;
    update();
}

void BoardWidget::setDragEnabled(bool enabled) {
    dragEnabled_ = enabled;
}

void BoardWidget::setUnitSelectedCallback(UnitSelectedCallback callback) {
    unitSelectedCallback_ = std::move(callback);
}

void BoardWidget::setBoardDropCallback(BoardDropCallback callback) {
    boardDropCallback_ = std::move(callback);
}

void BoardWidget::refreshFromState() {
    updateGeometry();
    update();
}

void BoardWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#f7f8fb"));

    for (int row = 0; row < game_->board().rows(); ++row) {
        for (int col = 0; col < game_->board().cols(); ++col) {
            const Position pos{row, col};
            const QRect cell = cellRect(pos);
            drawCellBase(painter, pos, cell);
        }
    }

    for (int row = 0; row < game_->board().rows(); ++row) {
        for (int col = 0; col < game_->board().cols(); ++col) {
            drawGrid(painter, cellRect(Position{row, col}));
        }
    }

    for (int row = 0; row < game_->board().rows(); ++row) {
        for (int col = 0; col < game_->board().cols(); ++col) {
            const Position pos{row, col};
            const auto id = game_->board().occupant(pos);
            if (id.has_value()) {
                drawUnit(painter, cellRect(pos).adjusted(5, 5, -5, -5), *id);
            }
        }
    }

    for (int row = 0; row < game_->board().rows(); ++row) {
        for (int col = 0; col < game_->board().cols(); ++col) {
            const Position pos{row, col};
            const auto id = game_->board().occupant(pos);
            if (id.has_value()) {
                drawSelectionOverlay(painter, cellRect(pos).adjusted(5, 5, -5, -5), *id);
            }
        }
    }

    for (int row = 0; row < game_->board().rows(); ++row) {
        for (int col = 0; col < game_->board().cols(); ++col) {
            const Position pos{row, col};
            drawHoverOverlay(painter, cellRect(pos), pos);
        }
    }

    for (int row = 0; row < game_->board().rows(); ++row) {
        for (int col = 0; col < game_->board().cols(); ++col) {
            const Position pos{row, col};
            drawDropOverlay(painter, cellRect(pos), pos);
        }
    }
}

void BoardWidget::mousePressEvent(QMouseEvent* event) {
    pressedCell_.reset();
    dropTargetCell_.reset();
    if (event->button() != Qt::LeftButton) {
        update();
        return;
    }

    const auto cell = cellAt(event->pos());
    hoverCell_ = cell;
    if (!cell.has_value()) {
        if (unitSelectedCallback_) {
            unitSelectedCallback_(std::nullopt);
        }
        update();
        return;
    }

    const auto id = game_->board().occupant(*cell);
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
        pressedCell_ = *cell;
        dragStartPosition_ = event->pos();
    }
    update();
}

void BoardWidget::mouseMoveEvent(QMouseEvent* event) {
    const auto currentCell = cellAt(event->pos());
    if (hoverCell_ != currentCell) {
        hoverCell_ = currentCell;
        update();
    }

    if (!pressedCell_.has_value()) {
        return;
    }
    if ((event->pos() - dragStartPosition_).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    const auto id = game_->board().occupant(*pressedCell_);
    if (!id.has_value() || !canStartDrag(*id)) {
        pressedCell_.reset();
        return;
    }

    UnitDragData data;
    data.unitId = *id;
    data.sourceType = DragSourceType::Board;
    data.boardPosition = *pressedCell_;

    auto* drag = new QDrag(this);
    auto* mime = new QMimeData();
    mime->setData(kUnitDragMimeType, encodeDragData(data));
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
    pressedCell_.reset();
    dropTargetCell_.reset();
    update();
}

void BoardWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(kUnitDragMimeType)) {
        event->acceptProposedAction();
    }
}

void BoardWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat(kUnitDragMimeType) && cellAt(event->position().toPoint()).has_value()) {
        dropTargetCell_ = cellAt(event->position().toPoint());
        update();
        event->acceptProposedAction();
        return;
    }
    dropTargetCell_.reset();
    update();
}

void BoardWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    dropTargetCell_.reset();
    update();
    event->accept();
}

void BoardWidget::dropEvent(QDropEvent* event) {
    UnitDragData data;
    const auto target = cellAt(event->position().toPoint());
    if (!target.has_value() || !decodeDragData(event->mimeData()->data(kUnitDragMimeType), data)) {
        dropTargetCell_.reset();
        update();
        event->ignore();
        return;
    }

    if (boardDropCallback_) {
        boardDropCallback_(data, *target);
    }
    dropTargetCell_.reset();
    update();
    event->acceptProposedAction();
}

void BoardWidget::leaveEvent(QEvent* event) {
    hoverCell_.reset();
    update();
    QWidget::leaveEvent(event);
}

std::optional<Position> BoardWidget::cellAt(const QPoint& point) const {
    const int col = (point.x() - kMargin) / kCellSize;
    const int row = (point.y() - kMargin) / kCellSize;
    const Position position{row, col};
    if (!game_->board().isInside(position)) {
        return std::nullopt;
    }
    if (!cellRect(position).contains(point)) {
        return std::nullopt;
    }
    return position;
}

QRect BoardWidget::cellRect(Position position) const {
    return QRect(kMargin + position.col * kCellSize, kMargin + position.row * kCellSize, kCellSize, kCellSize);
}

void BoardWidget::drawCellBase(QPainter& painter, Position position, const QRect& rect) const {
    painter.save();
    const QColor fill = game_->board().isEnemyHalf(position) ? QColor("#fde9e7") : QColor("#e8f1ff");
    painter.fillRect(rect, fill);
    painter.restore();
}

void BoardWidget::drawGrid(QPainter& painter, const QRect& rect) const {
    painter.save();
    painter.setPen(QPen(QColor("#7b8494"), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect);
    painter.restore();
}

void BoardWidget::drawUnit(QPainter& painter, const QRect& rect, UnitId id) const {
    painter.save();
    const Unit* unit = game_->unit(id);
    if (unit == nullptr) {
        painter.restore();
        return;
    }

    const QPixmap* pixmap = assets_ != nullptr ? assets_->pixmapFor(unit->visualKey()) : nullptr;
    if (pixmap != nullptr) {
        painter.drawPixmap(rect.adjusted(2, 2, -2, -12), *pixmap);
    } else {
        const QColor color = unit->owner() == Owner::PlayerCtrl ? QColor("#2f6fed") : QColor("#c83f3f");
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(rect.adjusted(2, 2, -2, -12), 6, 6);
        painter.setPen(Qt::white);
        painter.drawText(rect.adjusted(4, 4, -4, -16), Qt::AlignCenter, shortLabel(*unit, id));
    }

    painter.restore();
    drawHpManaBars(painter, rect, *unit);
}

void BoardWidget::drawHpManaBars(QPainter& painter, const QRect& rect, const Unit& unit) const {
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

void BoardWidget::drawSelectionOverlay(QPainter& painter, const QRect& rect, UnitId id) const {
    painter.save();
    if (selectedUnit_.has_value() && *selectedUnit_ == id) {
        painter.setPen(QPen(QColor("#ffb000"), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
    }
    painter.restore();
}

void BoardWidget::drawHoverOverlay(QPainter& painter, const QRect& rect, Position position) const {
    painter.save();
    if (hoverCell_.has_value() && *hoverCell_ == position) {
        painter.setPen(QPen(QColor(40, 40, 40, 90), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.adjusted(2, 2, -2, -2));
    }
    painter.restore();
}

void BoardWidget::drawDropOverlay(QPainter& painter, const QRect& rect, Position position) const {
    painter.save();
    if (dropTargetCell_.has_value() && *dropTargetCell_ == position) {
        painter.setPen(QPen(QColor(34, 139, 230, 190), 3, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.adjusted(3, 3, -3, -3));
    }
    painter.restore();
}

bool BoardWidget::canStartDrag(UnitId id) const {
    if (!dragEnabled_) {
        return false;
    }
    const Unit* unit = game_->unit(id);
    return unit != nullptr && unit->owner() == Owner::PlayerCtrl;
}

}  // namespace synera::gui
