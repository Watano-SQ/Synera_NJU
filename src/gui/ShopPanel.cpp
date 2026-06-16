#include "ShopPanel.h"

#include "AssetManager.h"
#include "PaintUtils.h"
#include "core/Catalog.h"

#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>

#include <algorithm>

namespace synera::gui {
namespace {

constexpr int kPanelWidth = 780;
constexpr int kPanelHeight = 190;
constexpr int kCardWidth = 120;
constexpr int kCardHeight = 71;
constexpr int kCardGap = 10;
constexpr int kCardStartX = 130;
constexpr int kCardY = 78;

}  // namespace

ShopPanel::ShopPanel(const GameState* game, AssetManager* assets, QWidget* parent)
    : QWidget(parent), game_(game), assets_(assets), refreshRect_(680, 25, 80, 32) {
    setFixedSize(kPanelWidth, kPanelHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    for (std::size_t i = 0; i < cardRects_.size(); ++i) {
        const int x = kCardStartX + static_cast<int>(i) * (kCardWidth + kCardGap);
        cardRects_[i] = QRect(x, kCardY, kCardWidth, kCardHeight);
    }
}

void ShopPanel::setPurchaseCallback(PurchaseCallback callback) {
    purchaseCallback_ = std::move(callback);
}

void ShopPanel::setRefreshCallback(RefreshCallback callback) {
    refreshCallback_ = std::move(callback);
}

void ShopPanel::refreshFromState() {
    update();
}

void ShopPanel::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect container = containerRect();
    painter.setPen(QPen(QColor("#3b1a0b"), 3));
    painter.setBrush(QColor("#7a3516"));
    painter.drawRoundedRect(container.adjusted(1, 1, -1, -1), 8, 8);

    const QRect counter = sunCounterRect();
    const QPixmap* sunCounter = assets_ != nullptr ? assets_->pixmapFor("ui/sun_counter") : nullptr;
    if (sunCounter != nullptr) {
        drawPixmapAspectFit(painter, counter, *sunCounter);
    } else {
        painter.setPen(QPen(QColor("#5b2d11"), 2));
        painter.setBrush(QColor("#d7a23a"));
        painter.drawRoundedRect(counter.adjusted(0, 0, -1, -1), 5, 5);
    }

    QFont goldFont = painter.font();
    goldFont.setBold(true);
    goldFont.setPointSize(11);
    painter.setFont(goldFont);
    painter.setPen(QColor("#2f2010"));
    painter.drawText(sunTextRect(), Qt::AlignCenter, QString::number(game_->player().gold()));

    for (std::size_t i = 0; i < cardRects_.size(); ++i) {
        drawShopCard(painter, i);
    }

    drawRefreshButton(painter);
}

void ShopPanel::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (game_->phase() != GamePhase::Prep) {
        return;
    }

    const QPoint point = event->pos();
    if (refreshRect_.contains(point)) {
        if (refreshCallback_) {
            refreshCallback_();
        }
        return;
    }

    const auto& offers = game_->shopOffers();
    for (std::size_t i = 0; i < cardRects_.size(); ++i) {
        if (cardRects_[i].contains(point) && i < offers.size() && offers[i].has_value() && canBuySlot(i)) {
            if (purchaseCallback_) {
                purchaseCallback_(i);
            }
            return;
        }
    }
}

QRect ShopPanel::containerRect() const {
    return QRect(0, 10, 780, 150);
}

QRect ShopPanel::sunCounterRect() const {
    return QRect(24, 32, 76, 26);
}

QRect ShopPanel::sunTextRect() const {
    const QRect counter = sunCounterRect();
    return QRect(counter.left() + 38, counter.top() + 3, counter.width() - 42, counter.height() - 6);
}

QRect ShopPanel::refreshRect() const {
    return refreshRect_;
}

void ShopPanel::drawRefreshButton(QPainter& painter) const {
    painter.save();
    const bool enabled = game_->phase() == GamePhase::Prep;
    painter.setPen(QPen(QColor("#2e1808"), 2));
    painter.setBrush(enabled ? QColor("#6b4a24") : QColor("#4a4034"));
    painter.drawRect(refreshRect_.adjusted(0, 0, -1, -1));

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(10);
    painter.setFont(font);
    painter.setPen(enabled ? QColor("#fff0bd") : QColor("#a79a87"));
    painter.drawText(refreshRect_, Qt::AlignCenter, QString::fromUtf8("刷新 2"));
    painter.restore();
}

void ShopPanel::drawShopCard(QPainter& painter, std::size_t index) const {
    painter.save();
    const QRect rect = cardRects_[index];
    const auto& offers = game_->shopOffers();
    if (index >= offers.size() || !offers[index].has_value()) {
        painter.setPen(QPen(QColor("#5c2a13"), 2));
        painter.setBrush(QColor("#2b1209"));
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
        painter.setPen(QColor("#8a6e5a"));
        painter.drawText(rect, Qt::AlignCenter, QStringLiteral("-"));
        painter.restore();
        return;
    }

    const UnitDefinition* definition = findUnitDefinition(offers[index]->definitionId);
    if (definition == nullptr) {
        painter.setPen(QPen(QColor("#5c2a13"), 2));
        painter.setBrush(QColor("#2b1209"));
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
        painter.setPen(QColor("#8a6e5a"));
        painter.drawText(rect, Qt::AlignCenter, QStringLiteral("?"));
        painter.restore();
        return;
    }

    const bool canBuy = canBuySlot(index);
    const std::string cardKey = "shop_cards/" + definition->definitionId + (canBuy ? "_available" : "_disabled");
    const QPixmap* card = assets_ != nullptr ? assets_->pixmapFor(cardKey) : nullptr;
    if (card != nullptr) {
        drawPixmapAspectFit(painter, rect, *card);
    } else {
        painter.setPen(QPen(QColor("#5c2a13"), 2));
        painter.setBrush(QColor("#2b1209"));
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
        painter.setPen(QColor("#d8c3a5"));
        painter.drawText(rect.adjusted(4, 0, -4, 0),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         QString::fromStdString(definition->name));
    }

    const QRect priceRect(rect.right() - 23, rect.bottom() - 18, 20, 14);
    QFont priceFont = painter.font();
    priceFont.setBold(true);
    priceFont.setPointSize(10);
    painter.setFont(priceFont);
    painter.setPen(QColor("#fff5bd"));
    painter.drawText(priceRect, Qt::AlignCenter, QString::number(offers[index]->cost));
    painter.restore();
}

bool ShopPanel::canBuySlot(std::size_t index) const {
    const auto& offers = game_->shopOffers();
    return game_->phase() == GamePhase::Prep && index < offers.size() && offers[index].has_value() &&
           game_->player().gold() >= offers[index]->cost;
}

}  // namespace synera::gui
