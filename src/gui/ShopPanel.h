#pragma once

#include "core/GameState.h"

#include <QRect>
#include <QWidget>

#include <array>
#include <functional>

class QMouseEvent;
class QPainter;

namespace synera::gui {

class AssetManager;

// 商店面板自绘 5 个商品槽、阳光数量和刷新按钮。
class ShopPanel : public QWidget {
public:
    // 商店面板通过回调告诉 MainWindow 用户点击了哪个槽位或刷新按钮。
    // 购买费用、金币是否足够、Bench 是否有空位等规则都由 GameState 决定。
    using PurchaseCallback = std::function<void(std::size_t)>;
    using RefreshCallback = std::function<void()>;

    explicit ShopPanel(const GameState* game, AssetManager* assets, QWidget* parent = nullptr);

    void setPurchaseCallback(PurchaseCallback callback);
    void setRefreshCallback(RefreshCallback callback);
    void refreshFromState();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    // 这些矩形把布局计算集中在一个地方，绘制和点击命中都复用相同结果。
    QRect containerRect() const;
    QRect sunCounterRect() const;
    QRect sunTextRect() const;
    QRect refreshRect() const;
    // 绘制函数只读取状态并呈现；不会在 paintEvent 中触发购买或刷新这类副作用。
    void drawRefreshButton(QPainter& painter) const;
    void drawShopCard(QPainter& painter, std::size_t index) const;
    // canBuySlot 只用于视觉禁用效果，真正购买仍由 GameState 再校验一次。
    bool canBuySlot(std::size_t index) const;

    // game_ 与 assets_ 由 MainWindow 持有，ShopPanel 只借用它们绘制当前商店。
    const GameState* game_;
    AssetManager* assets_;
    // cardRects_ 在绘制时更新，鼠标点击时用来反查命中的商店槽。
    std::array<QRect, 5> cardRects_{};
    QRect refreshRect_;
    PurchaseCallback purchaseCallback_;
    RefreshCallback refreshCallback_;
};

}  // namespace synera::gui
