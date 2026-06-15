#pragma once

#include "AssetManager.h"
#include "DragData.h"
#include "core/GameState.h"

#include <QPoint>
#include <QWidget>

#include <functional>
#include <optional>

class QPainter;

namespace synera::gui {

class BoardWidget : public QWidget {
public:
    using UnitSelectedCallback = std::function<void(std::optional<UnitId>)>;
    using BoardDropCallback = std::function<void(const UnitDragData&, Position)>;

    BoardWidget(const GameState* game, AssetManager* assets, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    void setSelectedUnit(std::optional<UnitId> unitId);
    void setDragEnabled(bool enabled);
    void setUnitSelectedCallback(UnitSelectedCallback callback);
    void setBoardDropCallback(BoardDropCallback callback);
    void refreshFromState();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    std::optional<Position> cellAt(const QPoint& point) const;
    QRect cellRect(Position position) const;
    void drawCellBase(QPainter& painter, Position position, const QRect& rect) const;
    void drawGrid(QPainter& painter, const QRect& rect) const;
    void drawUnit(QPainter& painter, const QRect& rect, UnitId id) const;
    void drawHpManaBars(QPainter& painter, const QRect& rect, const Unit& unit) const;
    void drawSelectionOverlay(QPainter& painter, const QRect& rect, UnitId id) const;
    void drawHoverOverlay(QPainter& painter, const QRect& rect, Position position) const;
    void drawDropOverlay(QPainter& painter, const QRect& rect, Position position) const;
    bool canStartDrag(UnitId id) const;

    const GameState* game_;
    AssetManager* assets_;
    bool dragEnabled_ = true;
    std::optional<UnitId> selectedUnit_;
    std::optional<Position> hoverCell_;
    std::optional<Position> dropTargetCell_;
    std::optional<Position> pressedCell_;
    QPoint dragStartPosition_;
    UnitSelectedCallback unitSelectedCallback_;
    BoardDropCallback boardDropCallback_;
};

}  // namespace synera::gui
