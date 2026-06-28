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
    QRect containerRect() const;
    QRect sunCounterRect() const;
    QRect sunTextRect() const;
    QRect refreshRect() const;
    void drawRefreshButton(QPainter& painter) const;
    void drawShopCard(QPainter& painter, std::size_t index) const;
    bool canBuySlot(std::size_t index) const;

    const GameState* game_;
    AssetManager* assets_;
    std::array<QRect, 5> cardRects_{};
    QRect refreshRect_;
    PurchaseCallback purchaseCallback_;
    RefreshCallback refreshCallback_;
};

}  // namespace synera::gui
