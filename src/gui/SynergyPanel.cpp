#include "SynergyPanel.h"

#include "AssetManager.h"
#include "core/Catalog.h"

#include <QHBoxLayout>
#include <QPixmap>
#include <QLayoutItem>

namespace synera::gui {

SynergyPanel::SynergyPanel(const GameState* game, AssetManager* assets, QWidget* parent)
    : QWidget(parent), game_(game), assets_(assets) {
    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("羁绊", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);
    listLayout_ = new QVBoxLayout();
    root->addLayout(listLayout_);
    refreshFromState();
}

void SynergyPanel::refreshFromState() {
    while (QLayoutItem* item = listLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    for (const SynergyStatus& status : game_->activeSynergies()) {
        const TraitDefinition* trait = findTraitDefinition(status.trait);
        const QString displayName = QString::fromStdString(trait != nullptr ? trait->displayName : status.trait);
        const QString threshold =
            status.active ? QString::number(status.activeThreshold) : QString("next %1").arg(status.nextThreshold);
        auto* row = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* icon = new QLabel(row);
        icon->setFixedSize(28, 28);
        if (trait != nullptr && assets_ != nullptr) {
            const QPixmap* pixmap = assets_->pixmapFor(trait->visualKey);
            if (pixmap != nullptr) {
                icon->setPixmap(pixmap->scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        auto* label = new QLabel(QString("%1  %2  %3\n%4")
                                     .arg(displayName)
                                     .arg(status.count)
                                     .arg(threshold)
                                     .arg(QString::fromStdString(status.effectDescription)),
                                 row);
        label->setWordWrap(true);
        label->setEnabled(status.active);
        row->setEnabled(status.active);
        rowLayout->addWidget(icon);
        rowLayout->addWidget(label, 1);
        listLayout_->addWidget(row);
    }
}

}  // namespace synera::gui
