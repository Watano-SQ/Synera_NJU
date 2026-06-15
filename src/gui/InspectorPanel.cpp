#include "InspectorPanel.h"

#include "core/Catalog.h"
#include "core/Types.h"

#include <QFormLayout>
#include <QStringList>
#include <QVBoxLayout>

namespace synera::gui {

InspectorPanel::InspectorPanel(const GameState* game, QWidget* parent) : QWidget(parent), game_(game) {
    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("单位详情", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    title->setFont(titleFont);
    root->addWidget(title);

    auto* form = new QFormLayout();
    nameValue_ = new QLabel("-", this);
    starValue_ = new QLabel("-", this);
    equipmentValue_ = new QLabel("-", this);
    archetypeValue_ = new QLabel("-", this);
    ownerValue_ = new QLabel("-", this);
    stateValue_ = new QLabel("-", this);
    hpValue_ = new QLabel("-", this);
    atkValue_ = new QLabel("-", this);
    rangeValue_ = new QLabel("-", this);
    manaValue_ = new QLabel("-", this);
    baseStatsValue_ = new QLabel("-", this);
    effectiveStatsValue_ = new QLabel("-", this);
    traitsValue_ = new QLabel("-", this);
    placementValue_ = new QLabel("-", this);
    visualKeyValue_ = new QLabel("-", this);

    traitsValue_->setWordWrap(true);
    baseStatsValue_->setWordWrap(true);
    effectiveStatsValue_->setWordWrap(true);
    equipmentValue_->setWordWrap(true);
    placementValue_->setWordWrap(true);
    visualKeyValue_->setWordWrap(true);

    form->addRow("名称", nameValue_);
    form->addRow("星级", starValue_);
    form->addRow("装备", equipmentValue_);
    form->addRow("技能类型", archetypeValue_);
    form->addRow("归属", ownerValue_);
    form->addRow("状态", stateValue_);
    form->addRow("生命", hpValue_);
    form->addRow("攻击", atkValue_);
    form->addRow("距离", rangeValue_);
    form->addRow("法力", manaValue_);
    form->addRow("基础属性", baseStatsValue_);
    form->addRow("最终属性", effectiveStatsValue_);
    form->addRow("羁绊", traitsValue_);
    form->addRow("位置", placementValue_);
    form->addRow("资源键", visualKeyValue_);
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
        starValue_->setText("-");
        equipmentValue_->setText("-");
        archetypeValue_->setText("-");
        ownerValue_->setText("-");
        stateValue_->setText("-");
        hpValue_->setText("-");
        atkValue_->setText("-");
        rangeValue_->setText("-");
        manaValue_->setText("-");
        baseStatsValue_->setText("-");
        effectiveStatsValue_->setText("-");
        traitsValue_->setText("-");
        placementValue_->setText("-");
        visualKeyValue_->setText("-");
        return;
    }

    nameValue_->setText(QString::fromStdString(unit->name()));
    starValue_->setText(QString::number(unit->star()));
    equipmentValue_->setText(itemText(*unit));
    archetypeValue_->setText(QString::fromStdString(unit->archetype()));
    ownerValue_->setText(QString::fromStdString(toString(unit->owner())));
    stateValue_->setText(QString::fromStdString(toString(unit->state())));
    hpValue_->setText(QString("%1 / %2").arg(unit->hp()).arg(unit->maxHp()));
    atkValue_->setText(QString::number(unit->atk()));
    rangeValue_->setText(QString::number(unit->range()));
    manaValue_->setText(QString("%1 / %2").arg(unit->mana()).arg(unit->maxMana()));
    baseStatsValue_->setText(QString::fromStdString(toString(unit->baseStats())));
    effectiveStatsValue_->setText(QString::fromStdString(toString(unit->effectiveStats())));
    traitsValue_->setText(traitsText(*unit));
    placementValue_->setText(QString::fromStdString(toString(unit->placement())));
    visualKeyValue_->setText(QString::fromStdString(displayVisualKey(*unit)));
}

QString InspectorPanel::traitsText(const Unit& unit) const {
    QStringList parts;
    for (const std::string& trait : unit.traits()) {
        const TraitDefinition* definition = findTraitDefinition(trait);
        parts << QString::fromStdString(definition != nullptr ? definition->displayName : trait);
    }
    return parts.isEmpty() ? "-" : parts.join(", ");
}

QString InspectorPanel::itemText(const Unit& unit) const {
    if (!unit.equippedItemId().has_value()) {
        return "-";
    }
    const ItemId itemId = *unit.equippedItemId();
    const auto instance = game_->item(itemId);
    if (!instance.has_value()) {
        return QString("#%1").arg(itemId);
    }
    const ItemDefinition* definition = findItemDefinition(instance->itemDefId);
    if (definition == nullptr) {
        return QString("#%1 %2").arg(itemId).arg(QString::fromStdString(instance->itemDefId));
    }
    return QString("#%1 %2").arg(itemId).arg(QString::fromStdString(definition->name));
}

}  // namespace synera::gui
