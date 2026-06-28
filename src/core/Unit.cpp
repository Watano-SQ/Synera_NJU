#include "Unit.h"

#include "SkillContext.h"

#include <algorithm>

namespace synera {

Unit::Unit(std::string name,
           Owner owner,
           int maxHp,
           int atk,
           int range,
           int maxMana,
           std::vector<std::string> traits,
           std::string visualKey)
    : Unit(name,
           name,
           owner,
           UnitStats{maxHp, atk, range, maxMana, 60, 20},
           std::move(traits),
           std::move(visualKey),
           "BasicUnit") {}

Unit::Unit(std::string definitionId,
           std::string name,
           Owner owner,
           UnitStats baseStats,
           std::vector<std::string> traits,
           std::string visualKey,
           std::string factoryKey)
    : definitionId_(std::move(definitionId)),
      name_(std::move(name)),
      visualKey_(std::move(visualKey)),
      factoryKey_(std::move(factoryKey)),
      owner_(owner),
      hp_(baseStats.maxHp),
      baseStats_(baseStats),
      effectiveStats_(baseStats),
      traits_(std::move(traits)) {}

const std::string& Unit::name() const {
    return name_;
}

const std::string& Unit::definitionId() const {
    return definitionId_;
}

const std::string& Unit::visualKey() const {
    return visualKey_;
}

const std::string& Unit::factoryKey() const {
    return factoryKey_;
}

Owner Unit::owner() const {
    return owner_;
}

const std::vector<std::string>& Unit::traits() const {
    return traits_;
}

int Unit::star() const {
    return star_;
}

std::uint64_t Unit::acquireSeq() const {
    return acquireSeq_;
}

const UnitStats& Unit::baseStats() const {
    return baseStats_;
}

const UnitStats& Unit::effectiveStats() const {
    return effectiveStats_;
}

std::optional<ItemId> Unit::equippedItemId() const {
    return equippedItemId_;
}

int Unit::hp() const {
    return hp_;
}

int Unit::maxHp() const {
    return effectiveStats_.maxHp;
}

int Unit::atk() const {
    return effectiveStats_.atk;
}

int Unit::range() const {
    return effectiveStats_.range;
}

int Unit::maxMana() const {
    return effectiveStats_.maxMana;
}

int Unit::initialMana() const {
    return effectiveStats_.initialMana;
}

int Unit::manaRegenPerSecond() const {
    return effectiveStats_.manaRegenPerSecond;
}

int Unit::skillManaCost() const {
    return effectiveStats_.skillManaCost;
}

int Unit::skillCooldownTicks() const {
    return effectiveStats_.skillCooldownTicks;
}

int Unit::attackInterval() const {
    return effectiveStats_.attackInterval;
}

int Unit::moveInterval() const {
    return effectiveStats_.moveInterval;
}

int Unit::mana() const {
    return mana_;
}

UnitState Unit::state() const {
    return state_;
}

const Placement& Unit::placement() const {
    return placement_;
}

bool Unit::isAlive() const {
    return state_ != UnitState::Dead && hp_ > 0;
}

void Unit::setHp(int hp) {
    // 生命降到 0 时立即把状态标成 Dead，后续棋盘清理会根据 isAlive 移除占用。
    hp_ = std::clamp(hp, 0, effectiveStats_.maxHp);
    if (hp_ == 0) {
        state_ = UnitState::Dead;
    }
}

void Unit::setMana(int mana) {
    mana_ = std::clamp(mana, 0, effectiveStats_.maxMana);
}

void Unit::setStar(int star) {
    star_ = std::clamp(star, 1, 2);
}

void Unit::setAcquireSeq(std::uint64_t acquireSeq) {
    acquireSeq_ = acquireSeq;
}

void Unit::setBaseStats(UnitStats stats) {
    baseStats_ = stats;
}

void Unit::setEffectiveStats(UnitStats stats) {
    // 所有最终属性都在这里兜底，防止装备或羁绊把数值推到不可用范围。
    stats.maxHp = std::max(1, stats.maxHp);
    stats.atk = std::max(0, stats.atk);
    stats.range = std::max(1, stats.range);
    stats.maxMana = std::max(10, stats.maxMana);
    stats.initialMana = std::clamp(stats.initialMana, 0, stats.maxMana);
    stats.manaRegenPerSecond = std::max(0, stats.manaRegenPerSecond);
    stats.skillManaCost = std::clamp(stats.skillManaCost, 0, stats.maxMana);
    stats.skillCooldownTicks = std::max(0, stats.skillCooldownTicks);
    stats.attackInterval = std::max(1, stats.attackInterval);
    stats.moveInterval = std::max(1, stats.moveInterval);

    const int oldMaxHp = effectiveStats_.maxHp;
    const int oldMaxMana = effectiveStats_.maxMana;
    effectiveStats_ = stats;
    if (effectiveStats_.maxHp != oldMaxHp) {
        // 最大生命变化时保留“增减的差值”，例如穿上加血装备会同步增加当前生命。
        hp_ = std::clamp(hp_ + (effectiveStats_.maxHp - oldMaxHp), 0, effectiveStats_.maxHp);
    } else {
        hp_ = std::clamp(hp_, 0, effectiveStats_.maxHp);
    }
    if (effectiveStats_.maxMana != oldMaxMana) {
        mana_ = std::clamp(mana_, 0, effectiveStats_.maxMana);
    }
}

void Unit::setEquippedItemId(std::optional<ItemId> itemId) {
    equippedItemId_ = itemId;
}

void Unit::setState(UnitState state) {
    state_ = state;
}

void Unit::setPlacement(Placement placement) {
    placement_ = std::move(placement);
}

std::string Unit::archetype() const {
    return "Unit";
}

bool Unit::hasActiveSkill() const {
    return false;
}

void Unit::castSkill(SkillContext&) {}

std::string BasicUnit::archetype() const {
    return "BasicUnit";
}

bool BasicUnit::hasActiveSkill() const {
    return false;
}

void BasicUnit::castSkill(SkillContext&) {}

std::string PeaBurst::archetype() const {
    return "PeaBurst";
}

bool PeaBurst::hasActiveSkill() const {
    return true;
}

void PeaBurst::castSkill(SkillContext& context) {
    // 豌豆爆发只关心当前目标，真正扣血由 GameState 之后统一应用。
    const auto target = context.targetId();
    if (target.has_value()) {
        context.dealDamage(*target, atk() * 2);
    }
}

std::string FumeLineCaster::archetype() const {
    return "FumeLineCaster";
}

bool FumeLineCaster::hasActiveSkill() const {
    return true;
}

void FumeLineCaster::castSkill(SkillContext& context) {
    // 大喷菇式技能以目标位置为锚点，攻击同一行所有敌方单位。
    const auto targetPosition = context.targetPosition();
    if (!targetPosition.has_value()) {
        return;
    }

    const auto targets = context.findUnitsInLine(*targetPosition, context.enemyOwner());
    for (UnitId id : targets) {
        context.dealDamage(id, atk());
    }
}

std::string SunHealer::archetype() const {
    return "SunHealer";
}

bool SunHealer::hasActiveSkill() const {
    return true;
}

void SunHealer::castSkill(SkillContext& context) {
    // 向日葵式治疗以自己为圆心，治疗半径内友军。
    const auto casterPosition = context.casterPosition();
    if (!casterPosition.has_value()) {
        return;
    }

    const auto allies = context.findAlliesInRadius(*casterPosition, 2);
    for (UnitId id : allies) {
        context.heal(id, atk());
    }
}

}  // namespace synera
