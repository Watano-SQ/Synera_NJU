#pragma once

#include "Types.h"

#include <optional>
#include <vector>

namespace synera {

class GameState;

// 技能产生的延迟效果。战斗循环会先收集所有效果，再统一结算。
struct CombatEffect {
    UnitId source = 0;
    UnitId target = 0;
    int amount = 0;
    bool grantsMana = false;
};

// 技能访问 GameState 的受控窗口。
// 派生 Unit 不直接修改棋盘和生命，而是通过这里查询目标并追加伤害/治疗事件。
class SkillContext {
public:
    SkillContext(GameState& game,
                 UnitId casterId,
                 std::optional<UnitId> targetId,
                 std::vector<CombatEffect>& damageEvents,
                 std::vector<CombatEffect>& healEvents);

    UnitId casterId() const;
    std::optional<UnitId> targetId() const;
    Owner casterOwner() const;
    Owner enemyOwner() const;
    std::optional<Position> casterPosition() const;
    std::optional<Position> targetPosition() const;

    std::vector<UnitId> findUnitsInLine(Position anchor, Owner owner) const;
    std::vector<UnitId> findAlliesInRadius(Position center, int radius) const;
    void dealDamage(UnitId targetId, int amount);
    void heal(UnitId targetId, int amount);

private:
    GameState& game_;
    UnitId casterId_;
    std::optional<UnitId> targetId_;
    std::vector<CombatEffect>& damageEvents_;
    std::vector<CombatEffect>& healEvents_;
};

}  // namespace synera
