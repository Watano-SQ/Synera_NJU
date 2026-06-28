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

// Bench 控件绘制备战区槽位，并把拖拽/选择动作通过回调交给 MainWindow。
class BenchWidget : public QWidget {
public:
    // 备战区只向外报告“选中某个 UnitId”或“有拖拽落到某个槽位”，不自己执行规则命令。
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
    // 绘制函数每次都从 GameState 读取槽位占用，保证购买、合成、撤回后立即反映最新状态。
    void paintEvent(QPaintEvent* event) override;
    // Bench 的拖拽来源只可能是玩家单位；真正是否能拖仍通过 MainWindow/PlacementController 判断。
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // slotAt/slotRect 是 Bench 的命中测试核心，绘制和鼠标交互共用它们避免偏移不一致。
    std::optional<int> slotAt(const QPoint& point) const;
    QRect slotRect(int slot) const;
    // 这些绘制函数按层次叠加：格子底图 -> 单位 -> 血蓝条 -> 选中/悬停/落点覆盖层。
    void drawCellBase(QPainter& painter, const QRect& rect) const;
    void drawGrid(QPainter& painter, const QRect& rect) const;
    void drawUnit(QPainter& painter, const QRect& rect, UnitId id) const;
    void drawHpManaBars(QPainter& painter, const QRect& rect, const Unit& unit) const;
    void drawSelectionOverlay(QPainter& painter, const QRect& rect, UnitId id) const;
    void drawHoverOverlay(QPainter& painter, const QRect& rect, int slot) const;
    void drawDropOverlay(QPainter& painter, const QRect& rect, int slot) const;
    bool canStartDrag(UnitId id) const;

    // BenchWidget 不拥有规则对象和资源对象，只读它们并把动作通过回调交给 MainWindow。
    const GameState* game_;
    AssetManager* assets_;
    // 战斗阶段关闭拖拽；准备阶段再允许玩家调整阵容。
    bool dragEnabled_ = true;
    // 这些 optional 只影响视觉反馈，不是游戏存档的一部分。
    std::optional<UnitId> selectedUnit_;
    std::optional<int> hoverSlot_;
    std::optional<int> dropTargetSlot_;
    std::optional<int> pressedSlot_;
    QPoint dragStartPosition_;
    UnitSelectedCallback unitSelectedCallback_;
    BenchDropCallback benchDropCallback_;
};

}  // namespace synera::gui
