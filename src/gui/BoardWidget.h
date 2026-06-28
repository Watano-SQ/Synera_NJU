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

// 棋盘控件只负责绘制和收集鼠标/拖拽事件，真正改状态交给回调和 PlacementController。
class BoardWidget : public QWidget {
public:
    // 回调把 QWidget 事件转交给 MainWindow；BoardWidget 本身不修改 GameState。
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
    // paintEvent 根据当前 GameState 一次性重画整张棋盘，避免局部状态和规则层不同步。
    void paintEvent(QPaintEvent* event) override;
    // 鼠标事件只负责选中、记录按下格和启动拖拽；落点合法性由 PlacementController 决定。
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    // dragEnter/dragMove/drop 读取 UnitDragData，并把目标格子交给 MainWindow 回调。
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // 坐标换算函数集中在这里，保证绘制、点击、拖拽命中使用同一套格子尺寸。
    std::optional<Position> cellAt(const QPoint& point) const;
    QRect cellRect(Position position) const;
    // 绘制拆成小函数，便于分别维护棋盘底色、单位贴图、血蓝条和各种交互覆盖层。
    void drawGrid(QPainter& painter, const QRect& rect) const;
    void drawUnit(QPainter& painter, const QRect& rect, UnitId id) const;
    void drawHpManaBars(QPainter& painter, const QRect& rect, const Unit& unit) const;
    void drawSelectionOverlay(QPainter& painter, const QRect& rect, UnitId id) const;
    void drawHoverOverlay(QPainter& painter, const QRect& rect, Position position) const;
    void drawDropOverlay(QPainter& painter, const QRect& rect, Position position) const;
    bool canStartDrag(UnitId id) const;

    // game_ 和 assets_ 都不归 BoardWidget 所有；它们由 MainWindow 创建并保证生命周期。
    const GameState* game_;
    AssetManager* assets_;
    // dragEnabled_ 通常在战斗阶段关闭，让 UI 行为和 GamePhase 保持一致。
    bool dragEnabled_ = true;
    // 以下状态只记录当前鼠标/拖拽视觉反馈，不写入规则层。
    std::optional<UnitId> selectedUnit_;
    std::optional<Position> hoverCell_;
    std::optional<Position> dropTargetCell_;
    std::optional<Position> pressedCell_;
    QPoint dragStartPosition_;
    UnitSelectedCallback unitSelectedCallback_;
    BoardDropCallback boardDropCallback_;
};

}  // namespace synera::gui
