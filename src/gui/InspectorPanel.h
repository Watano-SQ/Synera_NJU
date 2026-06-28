#pragma once

#include "core/GameState.h"

#include <QLabel>
#include <QWidget>

#include <optional>

namespace synera::gui {

class AssetManager;
class InspectorIconWidget;

// Inspector 显示当前选中单位的完整运行时信息，帮助调试属性、装备和位置。
class InspectorPanel : public QWidget {
public:
    explicit InspectorPanel(const GameState* game, AssetManager* assets, QWidget* parent = nullptr);

    // 选择变化后只记录 UnitId；具体文本、贴图和数值在 refreshFromState 中重新读取。
    void setSelectedUnit(std::optional<UnitId> unitId);
    void refreshFromState();

private:
    // traitsText/itemText 把规则层的稳定 id 翻译成面板上适合阅读的文本。
    QString traitsText(const Unit& unit) const;
    QString itemText(const Unit& unit) const;

    // Inspector 只借用 GameState/AssetManager，不修改任何游戏规则状态。
    const GameState* game_;
    AssetManager* assets_;
    // 没有选中单位时，面板显示空态；选中的 id 如果已被合成/删除，刷新时也会安全处理。
    std::optional<UnitId> selectedUnit_;
    InspectorIconWidget* unitIcon_;
    InspectorIconWidget* itemIcon_;
    QLabel* nameValue_;
    QLabel* starValue_;
    QLabel* equipmentValue_;
    QLabel* archetypeValue_;
    QLabel* ownerValue_;
    QLabel* stateValue_;
    QLabel* hpValue_;
    QLabel* atkValue_;
    QLabel* rangeValue_;
    QLabel* manaValue_;
    QLabel* initialManaValue_;
    QLabel* manaRegenValue_;
    QLabel* skillManaCostValue_;
    QLabel* skillCooldownValue_;
    QLabel* baseStatsValue_;
    QLabel* effectiveStatsValue_;
    QLabel* traitsValue_;
    QLabel* placementValue_;
    QLabel* visualKeyValue_;
};

}  // namespace synera::gui
