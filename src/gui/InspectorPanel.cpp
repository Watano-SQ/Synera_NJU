#include "InspectorPanel.h"

#include "core/Types.h"

#include <QFormLayout>
#include <QStringList>
#include <QVBoxLayout>

namespace synera::gui {

InspectorPanel::InspectorPanel(const GameState* game, QWidget* parent) : QWidget(parent), game_(game) {
    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("Unit Inspector", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    title->setFont(titleFont);
    root->addWidget(title);

    auto* form = new QFormLayout();
    nameValue_ = new QLabel("-", this);
    ownerValue_ = new QLabel("-", this);
    hpValue_ = new QLabel("-", this);
    atkValue_ = new QLabel("-", this);
    rangeValue_ = new QLabel("-", this);
    manaValue_ = new QLabel("-", this);
    traitsValue_ = new QLabel("-", this);
    placementValue_ = new QLabel("-", this);
    visualKeyValue_ = new QLabel("-", this);

    traitsValue_->setWordWrap(true);
    placementValue_->setWordWrap(true);
    visualKeyValue_->setWordWrap(true);

    form->addRow("Name", nameValue_);
    form->addRow("Owner", ownerValue_);
    form->addRow("HP", hpValue_);
    form->addRow("ATK", atkValue_);
    form->addRow("Range", rangeValue_);
    form->addRow("Mana", manaValue_);
    form->addRow("Traits", traitsValue_);
    form->addRow("Placement", placementValue_);
    form->addRow("Visual Key", visualKeyValue_);
    root->addLayout(form);
    root->addStretch();
    refreshFromState();
}

void InspectorPanel::setSelectedUnit(std::optional<UnitId> unitId) {
    selectedUnit_ = unitId;
    refreshFromState();
}

void InspectorPanel::refreshFromState() {
    const Unit* unit = selectedUnit_.has_value() ? game_->unit(*selectedUnit_) : nullptr;
    if (unit == nullptr) {
        nameValue_->setText("-");
        ownerValue_->setText("-");
        hpValue_->setText("-");
        atkValue_->setText("-");
        rangeValue_->setText("-");
        manaValue_->setText("-");
        traitsValue_->setText("-");
        placementValue_->setText("-");
        visualKeyValue_->setText("-");
        return;
    }

    nameValue_->setText(QString::fromStdString(unit->name()));
    ownerValue_->setText(QString::fromStdString(toString(unit->owner())));
    hpValue_->setText(QString("%1 / %2").arg(unit->hp()).arg(unit->maxHp()));
    atkValue_->setText(QString::number(unit->atk()));
    rangeValue_->setText(QString::number(unit->range()));
    manaValue_->setText(QString("%1 / %2").arg(unit->mana()).arg(unit->maxMana()));
    traitsValue_->setText(traitsText(*unit));
    placementValue_->setText(QString::fromStdString(toString(unit->placement())));
    visualKeyValue_->setText(QString::fromStdString(unit->visualKey()));
}

QString InspectorPanel::traitsText(const Unit& unit) const {
    QStringList parts;
    for (const std::string& trait : unit.traits()) {
        parts << QString::fromStdString(trait);
    }
    return parts.isEmpty() ? "-" : parts.join(", ");
}

}  // namespace synera::gui
