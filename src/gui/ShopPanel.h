#pragma once

#include "core/GameState.h"

#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include <array>
#include <functional>

namespace synera::gui {

class AssetManager;

class ShopPanel : public QWidget {
public:
    using PurchaseCallback = std::function<void(std::size_t)>;
    using RefreshCallback = std::function<void()>;

    explicit ShopPanel(const GameState* game, AssetManager* assets, QWidget* parent = nullptr);

    void setPurchaseCallback(PurchaseCallback callback);
    void setRefreshCallback(RefreshCallback callback);
    void refreshFromState();

private:
    const GameState* game_;
    AssetManager* assets_;
    std::array<QLabel*, 5> offerIcons_{};
    std::array<QLabel*, 5> offerLabels_{};
    std::array<QPushButton*, 5> purchaseButtons_{};
    QPushButton* refreshButton_ = nullptr;
    PurchaseCallback purchaseCallback_;
    RefreshCallback refreshCallback_;
};

}  // namespace synera::gui
