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

class BenchWidget : public QWidget {
public:
    using UnitSelectedCallback = std::function<void(std::optional<UnitId>)>;
    using BenchDropCallback = std::function<void(const UnitDragData&, int)>;

    BenchWidget(const GameState* game, AssetManager* assets, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    void setSelectedUnit(std::optional<UnitId> unitId);
    void setDragEnabled(bool enabled);
    void setUnitSelectedCallback(UnitSelectedCallback callback);
    void setBenchDropCallback(BenchDropCallback callback);
    void refreshFromState();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    std::optional<int> slotAt(const QPoint& point) const;
    QRect slotRect(int slot) const;
    void drawUnit(QPainter& painter, const QRect& rect, UnitId id) const;
    bool canStartDrag(UnitId id) const;

    const GameState* game_;
    AssetManager* assets_;
    bool dragEnabled_ = true;
    std::optional<UnitId> selectedUnit_;
    std::optional<int> pressedSlot_;
    QPoint dragStartPosition_;
    UnitSelectedCallback unitSelectedCallback_;
    BenchDropCallback benchDropCallback_;
};

}  // namespace synera::gui
