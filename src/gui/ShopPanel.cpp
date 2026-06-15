#include "ShopPanel.h"

#include "AssetManager.h"
#include "core/Catalog.h"

#include <QGridLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace synera::gui {
namespace {

QString traitsText(const UnitDefinition& definition) {
    QStringList traits;
    for (const std::string& trait : definition.traits) {
        const TraitDefinition* traitDefinition = findTraitDefinition(trait);
        traits << QString::fromStdString(traitDefinition != nullptr ? traitDefinition->displayName : trait);
    }
    return traits.join(", ");
}

QPixmap scaledCentered(const QPixmap& source, QSize size) {
    return source.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

}  // namespace

class ShopCardButton : public QPushButton {
public:
    explicit ShopCardButton(QWidget* parent = nullptr) : QPushButton(parent) {
        setFlat(true);
        setMinimumSize(sizeHint());
        setCursor(Qt::PointingHandCursor);
    }

    QSize sizeHint() const override {
        return QSize(104, 148);
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
        painter.setPen(QPen(isEnabled() ? QColor("#6f4d21") : QColor("#6b6258"), 2));
        painter.setBrush(isEnabled() ? QColor("#efe0ad") : QColor("#9c9587"));
        painter.drawRoundedRect(body, 7, 7);

        if (!hasOffer_) {
            painter.setPen(QColor("#5f5a51"));
            painter.drawText(body, Qt::AlignCenter, "-");
            return;
        }

        const QRect cardRect = body.adjusted(8, 8, -8, -36);
        if (!card_.isNull()) {
            const QPixmap scaled = scaledCentered(card_, cardRect.size());
            const QPoint topLeft(cardRect.center().x() - scaled.width() / 2, cardRect.center().y() - scaled.height() / 2);
            painter.drawPixmap(topLeft, scaled);
        } else {
            painter.setPen(QPen(QColor("#705023"), 1));
            painter.setBrush(QColor("#f5e8b8"));
            painter.drawRoundedRect(cardRect, 5, 5);
            if (!fallbackIcon_.isNull()) {
                const QRect iconRect(cardRect.left() + 12, cardRect.top() + 12, cardRect.width() - 24, 48);
                const QPixmap scaled = scaledCentered(fallbackIcon_, iconRect.size());
                const QPoint topLeft(iconRect.center().x() - scaled.width() / 2, iconRect.center().y() - scaled.height() / 2);
                painter.drawPixmap(topLeft, scaled);
            }
            painter.setPen(QColor("#2d2719"));
            painter.drawText(cardRect.adjusted(4, 62, -4, -18), Qt::AlignHCenter | Qt::TextWordWrap, title_);
            painter.setPen(QColor("#5f5134"));
            painter.drawText(cardRect.adjusted(4, cardRect.height() - 18, -4, -2), Qt::AlignCenter, traits_);
        }

        if (!isEnabled()) {
            painter.fillRect(cardRect, QColor(35, 35, 35, 95));
        }

        const QRect priceRect(body.left() + 10, body.bottom() - 28, body.width() - 20, 22);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(47, 36, 16, 190));
        painter.drawRoundedRect(priceRect, 4, 4);
        const QRect sunRect(priceRect.left() + 6, priceRect.top() + 2, 18, 18);
        if (!sunIcon_.isNull()) {
            painter.drawPixmap(sunRect, sunIcon_);
        }
        painter.setPen(QColor("#fff4b8"));
        painter.drawText(priceRect.adjusted(28, 0, -6, 0), Qt::AlignVCenter | Qt::AlignRight, QString::number(cost_));
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
    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    titleArt_ = new QLabel("戴夫的植物商店", this);
    titleArt_->setAlignment(Qt::AlignCenter);
    titleArt_->setMinimumHeight(44);
    QFont titleFont = titleArt_->font();
    titleFont.setBold(true);
    titleArt_->setFont(titleFont);
    if (assets_ != nullptr) {
        const QPixmap* pixmap = assets_->pixmapFor("ui/plant_shop");
        if (pixmap != nullptr) {
            titleArt_->setPixmap(pixmap->scaled(220, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    root->addWidget(titleArt_);

    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    for (std::size_t i = 0; i < offerCards_.size(); ++i) {
        offerCards_[i] = new ShopCardButton(this);
        connect(offerCards_[i], &QPushButton::clicked, this, [this, i]() {
            if (purchaseCallback_) {
                purchaseCallback_(i);
            }
        });
        grid->addWidget(offerCards_[i], static_cast<int>(i / 2), static_cast<int>(i % 2));
    }
    root->addLayout(grid);

    refreshButton_ = new QPushButton("刷新 2", this);
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
    root->addWidget(refreshButton_);
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
