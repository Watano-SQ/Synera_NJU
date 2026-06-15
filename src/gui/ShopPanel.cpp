#include "ShopPanel.h"

#include "AssetManager.h"
#include "core/Catalog.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace synera::gui {
namespace {

QRect aspectFitRect(const QRect& target, const QSize& sourceSize) {
    if (sourceSize.isEmpty() || target.isEmpty()) {
        return QRect();
    }
    const QSize scaled = sourceSize.scaled(target.size(), Qt::KeepAspectRatio);
    return QRect(QPoint(target.center().x() - scaled.width() / 2,
                       target.center().y() - scaled.height() / 2),
                 scaled);
}

void drawPixmapAspectFit(QPainter& painter, const QRect& target, const QPixmap& pixmap) {
    if (pixmap.isNull() || target.isEmpty()) {
        return;
    }
    painter.drawPixmap(aspectFitRect(target, pixmap.size()), pixmap);
}

QString traitsText(const UnitDefinition& definition) {
    QStringList traits;
    for (const std::string& trait : definition.traits) {
        const TraitDefinition* traitDefinition = findTraitDefinition(trait);
        traits << QString::fromStdString(traitDefinition != nullptr ? traitDefinition->displayName : trait);
    }
    return traits.join(", ");
}

}  // namespace

class ShopCardButton : public QPushButton {
public:
    explicit ShopCardButton(QWidget* parent = nullptr) : QPushButton(parent) {
        setFlat(true);
        setFixedSize(sizeHint());
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
    }

    QSize sizeHint() const override {
        return QSize(112, 86);
    }

    void setOffer(QString title,
                  QString traits,
                  int cost,
                  const QPixmap* card,
                  const QPixmap* fallbackIcon,
                  const QPixmap* sunIcon,
                  bool hasOffer,
                  bool canBuy) {
        title_ = std::move(title);
        traits_ = std::move(traits);
        cost_ = cost;
        card_ = card != nullptr ? *card : QPixmap();
        fallbackIcon_ = fallbackIcon != nullptr ? *fallbackIcon : QPixmap();
        sunIcon_ = sunIcon != nullptr ? *sunIcon : QPixmap();
        hasOffer_ = hasOffer;
        setEnabled(hasOffer && canBuy);
        setCursor(isEnabled() ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect body = rect().adjusted(2, 2, -2, -2);
        painter.setPen(QPen(isEnabled() ? QColor("#6f4d21") : QColor("#4d453b"), 2));
        painter.setBrush(isEnabled() ? QColor("#d7b061") : QColor("#5f5a51"));
        painter.drawRoundedRect(body, 7, 7);

        if (!hasOffer_) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(24, 21, 18, 170));
            painter.drawRoundedRect(body.adjusted(6, 6, -6, -22), 5, 5);
            painter.setPen(QColor("#8a8173"));
            painter.drawText(body, Qt::AlignCenter, "-");
            return;
        }

        const QRect cardRect(body.left() + 3, body.top() + 3, body.width() - 6, 64);
        if (!card_.isNull()) {
            drawPixmapAspectFit(painter, cardRect, card_);
        } else {
            painter.setPen(QPen(QColor("#705023"), 1));
            painter.setBrush(QColor("#f5e8b8"));
            painter.drawRoundedRect(cardRect, 5, 5);
            if (!fallbackIcon_.isNull()) {
                drawPixmapAspectFit(painter, cardRect.adjusted(20, 8, -20, -12), fallbackIcon_);
            }
            painter.setPen(QColor("#2d2719"));
            painter.drawText(cardRect.adjusted(4, 42, -4, -2), Qt::AlignHCenter | Qt::TextWordWrap, title_);
            painter.setPen(QColor("#5f5134"));
            painter.drawText(cardRect.adjusted(4, cardRect.height() - 16, -4, -2), Qt::AlignCenter, traits_);
        }

        if (!isEnabled()) {
            painter.fillRect(cardRect, QColor(35, 35, 35, 65));
        }

        const QRect priceRect(body.right() - 50, body.bottom() - 18, 46, 16);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(47, 36, 16, 190));
        painter.drawRoundedRect(priceRect, 3, 3);
        if (!sunIcon_.isNull()) {
            drawPixmapAspectFit(painter, QRect(priceRect.left() + 3, priceRect.top() + 2, 16, 12), sunIcon_);
        }
        painter.setPen(QColor("#fff4b8"));
        painter.drawText(priceRect.adjusted(20, 0, -4, 0), Qt::AlignVCenter | Qt::AlignRight, QString::number(cost_));
    }

private:
    QString title_;
    QString traits_;
    int cost_ = 0;
    QPixmap card_;
    QPixmap fallbackIcon_;
    QPixmap sunIcon_;
    bool hasOffer_ = false;
};

ShopPanel::ShopPanel(const GameState* game, AssetManager* assets, QWidget* parent)
    : QWidget(parent), game_(game), assets_(assets) {
    setFixedHeight(188);
    setMinimumWidth(600);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 6);
    root->setSpacing(8);

    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);

    titleArt_ = new QLabel("Plant Shop", this);
    titleArt_->setAlignment(Qt::AlignCenter);
    titleArt_->setFixedSize(260, 52);
    QFont titleFont = titleArt_->font();
    titleFont.setBold(true);
    titleArt_->setFont(titleFont);
    if (assets_ != nullptr) {
        const QPixmap* pixmap = assets_->pixmapFor("ui/plant_shop");
        if (pixmap != nullptr) {
            titleArt_->setPixmap(pixmap->scaled(titleArt_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }

    refreshButton_ = new QPushButton("Refresh 2", this);
    refreshButton_->setFixedHeight(30);
    refreshButton_->setMinimumWidth(116);
    refreshButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    if (assets_ != nullptr) {
        if (const QPixmap* pixmap = assets_->pixmapFor("ui/button")) {
            refreshButton_->setIcon(QIcon(*pixmap));
            refreshButton_->setIconSize(QSize(48, 24));
        }
    }
    connect(refreshButton_, &QPushButton::clicked, this, [this]() {
        if (refreshCallback_) {
            refreshCallback_();
        }
    });

    titleRow->addWidget(titleArt_, 0, Qt::AlignLeft | Qt::AlignVCenter);
    titleRow->addStretch();
    titleRow->addWidget(refreshButton_, 0, Qt::AlignRight | Qt::AlignVCenter);
    root->addLayout(titleRow);

    auto* cardRow = new QHBoxLayout();
    cardRow->setContentsMargins(0, 0, 0, 0);
    cardRow->setSpacing(6);
    for (std::size_t i = 0; i < offerCards_.size(); ++i) {
        offerCards_[i] = new ShopCardButton(this);
        connect(offerCards_[i], &QPushButton::clicked, this, [this, i]() {
            if (purchaseCallback_) {
                purchaseCallback_(i);
            }
        });
        cardRow->addWidget(offerCards_[i], 0, Qt::AlignLeft | Qt::AlignTop);
    }
    cardRow->addStretch();
    root->addLayout(cardRow);
    refreshFromState();
}

void ShopPanel::setPurchaseCallback(PurchaseCallback callback) {
    purchaseCallback_ = std::move(callback);
}

void ShopPanel::setRefreshCallback(RefreshCallback callback) {
    refreshCallback_ = std::move(callback);
}

void ShopPanel::refreshFromState() {
    const bool canOperate = game_->phase() == GamePhase::Prep;
    const auto& offers = game_->shopOffers();
    const QPixmap* sunIcon = assets_ != nullptr ? assets_->pixmapFor("ui/sun_counter") : nullptr;
    for (std::size_t i = 0; i < offerCards_.size(); ++i) {
        if (i >= offers.size() || !offers[i].has_value()) {
            offerCards_[i]->setOffer("-", {}, 0, nullptr, nullptr, sunIcon, false, false);
            continue;
        }
        const UnitDefinition* definition = findUnitDefinition(offers[i]->definitionId);
        if (definition == nullptr) {
            offerCards_[i]->setOffer("Unknown", {}, offers[i]->cost, nullptr, nullptr, sunIcon, true, false);
            continue;
        }

        const bool canBuy = canOperate && game_->player().gold() >= offers[i]->cost;
        const std::string cardKey = "shop_cards/" + definition->definitionId + (canBuy ? "_available" : "_disabled");
        const std::string fallbackKey =
            !definition->star1VisualKey.empty() ? definition->star1VisualKey : definition->visualKey;
        const QPixmap* card = assets_ != nullptr ? assets_->pixmapFor(cardKey) : nullptr;
        const QPixmap* fallback = assets_ != nullptr ? assets_->pixmapFor(fallbackKey) : nullptr;
        offerCards_[i]->setOffer(QString::fromStdString(definition->name),
                                 traitsText(*definition),
                                 offers[i]->cost,
                                 card,
                                 fallback,
                                 sunIcon,
                                 true,
                                 canBuy);
    }
    refreshButton_->setEnabled(canOperate);
}

}  // namespace synera::gui
