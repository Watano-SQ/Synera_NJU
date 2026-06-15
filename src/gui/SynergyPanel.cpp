#include "SynergyPanel.h"

#include <QLayoutItem>

namespace synera::gui {

SynergyPanel::SynergyPanel(const GameState* game, QWidget* parent) : QWidget(parent), game_(game) {
    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("Synergies", this);
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
        const QString threshold =
            status.active ? QString::number(status.activeThreshold) : QString("next %1").arg(status.nextThreshold);
        auto* label = new QLabel(QString("%1  %2  %3\n%4")
                                     .arg(QString::fromStdString(status.trait))
                                     .arg(status.count)
                                     .arg(threshold)
                                     .arg(QString::fromStdString(status.effectDescription)),
                                 this);
        label->setWordWrap(true);
        label->setEnabled(status.active);
        listLayout_->addWidget(label);
    }
}

}  // namespace synera::gui
