#include "ShopPanel.h"

#include "core/Catalog.h"

#include <QGridLayout>
#include <QStringList>
#include <QVBoxLayout>

namespace synera::gui {
namespace {

QString traitsText(const UnitDefinition& definition) {
    QStringList traits;
    for (const std::string& trait : definition.traits) {
        traits << QString::fromStdString(trait);
    }
    return traits.join(", ");
}

}  // namespace

ShopPanel::ShopPanel(const GameState* game, QWidget* parent) : QWidget(parent), game_(game) {
    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("Shop", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto* grid = new QGridLayout();
    for (std::size_t i = 0; i < offerLabels_.size(); ++i) {
        offerLabels_[i] = new QLabel("-", this);
        offerLabels_[i]->setWordWrap(true);
        purchaseButtons_[i] = new QPushButton("Buy", this);
        connect(purchaseButtons_[i], &QPushButton::clicked, this, [this, i]() {
            if (purchaseCallback_) {
                purchaseCallback_(i);
            }
        });
        grid->addWidget(offerLabels_[i], static_cast<int>(i), 0);
        grid->addWidget(purchaseButtons_[i], static_cast<int>(i), 1);
    }
    root->addLayout(grid);

    refreshButton_ = new QPushButton("Refresh 2g", this);
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
            offerLabels_[i]->setText("-");
            purchaseButtons_[i]->setEnabled(false);
            continue;
        }
        const UnitDefinition* definition = findUnitDefinition(offers[i]->definitionId);
        if (definition == nullptr) {
            offerLabels_[i]->setText("Unknown");
            purchaseButtons_[i]->setEnabled(false);
            continue;
        }
        offerLabels_[i]->setText(QString("%1  %2g\n%3")
                                     .arg(QString::fromStdString(definition->name))
                                     .arg(offers[i]->cost)
                                     .arg(traitsText(*definition)));
        purchaseButtons_[i]->setEnabled(canOperate);
    }
    refreshButton_->setEnabled(canOperate);
}

}  // namespace synera::gui
