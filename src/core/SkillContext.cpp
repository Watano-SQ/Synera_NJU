#include "SkillContext.h"

#include "GameState.h"
#include "Unit.h"

#include <algorithm>

namespace synera {

namespace {

std::optional<Position> boardPositionOf(const Unit* unit) {
    // 技能只对棋盘上的单位有空间概念，Bench 中的单位没有 Position。
    if (unit == nullptr || unit->placement().kind != PlacementKind::BoardCell) {
        return std::nullopt;
    }
    return unit->placement().boardCell;
}

int distanceSquared(Position a, Position b) {
    const int dr = a.row - b.row;
    const int dc = a.col - b.col;
    return dr * dr + dc * dc;
}

}  // namespace

SkillContext::SkillContext(GameState& game,
                           UnitId casterId,
                           std::optional<UnitId> targetId,
                           std::vector<CombatEffect>& damageEvents,
                           std::vector<CombatEffect>& healEvents)
    : game_(game),
      casterId_(casterId),
      targetId_(targetId),
      damageEvents_(damageEvents),
      healEvents_(healEvents) {}

UnitId SkillContext::casterId() const {
    return casterId_;
}

std::optional<UnitId> SkillContext::targetId() const {
    return targetId_;
}

Owner SkillContext::casterOwner() const {
    const Unit* caster = game_.unit(casterId_);
    return caster != nullptr ? caster->owner() : Owner::PlayerCtrl;
}

Owner SkillContext::enemyOwner() const {
    return casterOwner() == Owner::PlayerCtrl ? Owner::EnemyCtrl : Owner::PlayerCtrl;
}

std::optional<Position> SkillContext::casterPosition() const {
    return boardPositionOf(game_.unit(casterId_));
}

std::optional<Position> SkillContext::targetPosition() const {
    if (!targetId_.has_value()) {
        return std::nullopt;
    }
    return boardPositionOf(game_.unit(*targetId_));
}

std::vector<UnitId> SkillContext::findUnitsInLine(Position anchor, Owner owner) const {
    std::vector<UnitId> result;
    // 行技能按 anchor.row 扫描整行，只返回指定 owner 的存活单位。
    for (int col = 0; col < game_.board().cols(); ++col) {
        const auto occupant = game_.board().occupant(Position{anchor.row, col});
        if (!occupant.has_value()) {
            continue;
        }

        const Unit* unit = game_.unit(*occupant);
        if (unit != nullptr && unit->owner() == owner && unit->isAlive()) {
            result.push_back(*occupant);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<UnitId> SkillContext::findAlliesInRadius(Position center, int radius) const {
    std::vector<UnitId> result;
    const int radiusSq = radius * radius;
    const Owner owner = casterOwner();
    // 使用距离平方避免开方；半径是欧氏距离意义上的圆形范围。
    for (int row = 0; row < game_.board().rows(); ++row) {
        for (int col = 0; col < game_.board().cols(); ++col) {
            const Position position{row, col};
            const auto occupant = game_.board().occupant(position);
            if (!occupant.has_value()) {
                continue;
            }

            const Unit* unit = game_.unit(*occupant);
            if (unit == nullptr || unit->owner() != owner || !unit->isAlive()) {
                continue;
            }

            if (distanceSquared(center, position) <= radiusSq) {
                result.push_back(*occupant);
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void SkillContext::dealDamage(UnitId targetId, int amount) {
    if (amount <= 0) {
        return;
    }
    // 只追加事件，不立即扣血，保证同一 tick 内的效果按统一顺序结算。
    damageEvents_.push_back(CombatEffect{casterId_, targetId, amount, false});
}

void SkillContext::heal(UnitId targetId, int amount) {
    if (amount <= 0) {
        return;
    }
    // 治疗同样延迟到 GameState::applyCombatEffects，方便统一处理 healer 羁绊。
    healEvents_.push_back(CombatEffect{casterId_, targetId, amount, false});
}

}  // namespace synera
