#include "ShopPanel.h"

#include "AssetManager.h"
#include "core/Catalog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QStringList>
#include <QVBoxLayout>

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

}  // namespace

ShopPanel::ShopPanel(const GameState* game, AssetManager* assets, QWidget* parent)
    : QWidget(parent), game_(game), assets_(assets) {
    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("戴夫的植物商店", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto* grid = new QGridLayout();
    for (std::size_t i = 0; i < offerLabels_.size(); ++i) {
        offerIcons_[i] = new QLabel(this);
        offerIcons_[i]->setFixedSize(40, 40);
        offerIcons_[i]->setScaledContents(true);
        offerLabels_[i] = new QLabel("-", this);
        offerLabels_[i]->setWordWrap(true);
        purchaseButtons_[i] = new QPushButton("种下", this);
        connect(purchaseButtons_[i], &QPushButton::clicked, this, [this, i]() {
            if (purchaseCallback_) {
                purchaseCallback_(i);
            }
        });
        grid->addWidget(offerIcons_[i], static_cast<int>(i), 0);
        grid->addWidget(offerLabels_[i], static_cast<int>(i), 1);
        grid->addWidget(purchaseButtons_[i], static_cast<int>(i), 2);
    }
    root->addLayout(grid);

    refreshButton_ = new QPushButton("刷新 2 阳光", this);
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
    for (std::size_t i = 0; i < offerLabels_.size(); ++i) {
        if (i >= offers.size() || !offers[i].has_value()) {
            offerIcons_[i]->clear();
            offerLabels_[i]->setText("-");
            purchaseButtons_[i]->setEnabled(false);
            continue;
        }
        const UnitDefinition* definition = findUnitDefinition(offers[i]->definitionId);
        if (definition == nullptr) {
            offerIcons_[i]->clear();
            offerLabels_[i]->setText("Unknown");
            purchaseButtons_[i]->setEnabled(false);
            continue;
        }
        const QPixmap* pixmap = assets_ != nullptr ? assets_->pixmapFor(definition->visualKey) : nullptr;
        offerIcons_[i]->setPixmap(pixmap != nullptr ? pixmap->scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                                    : QPixmap());
        offerLabels_[i]->setText(QString("%1  %2 阳光\n%3")
                                     .arg(QString::fromStdString(definition->name))
                                     .arg(offers[i]->cost)
                                     .arg(traitsText(*definition)));
        purchaseButtons_[i]->setEnabled(canOperate);
    }
    refreshButton_->setEnabled(canOperate);
}

}  // namespace synera::gui
