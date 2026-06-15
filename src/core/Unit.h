#pragma once

#include "Types.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace synera {

class SkillContext;

class Unit {
public:
    Unit(std::string name,
         Owner owner,
         int maxHp,
         int atk,
         int range,
         int maxMana,
         std::vector<std::string> traits = {},
         std::string visualKey = {});
    Unit(std::string definitionId,
         std::string name,
         Owner owner,
         UnitStats baseStats,
         std::vector<std::string> traits,
         std::string visualKey,
         std::string factoryKey);
    virtual ~Unit() = default;

    Unit(const Unit&) = delete;
    Unit& operator=(const Unit&) = delete;

    const std::string& name() const;
    const std::string& definitionId() const;
    const std::string& visualKey() const;
    const std::string& factoryKey() const;
    Owner owner() const;
    const std::vector<std::string>& traits() const;
    int star() const;
    std::uint64_t acquireSeq() const;
    const UnitStats& baseStats() const;
    const UnitStats& effectiveStats() const;
    std::optional<ItemId> equippedItemId() const;

    int hp() const;
    int maxHp() const;
    int atk() const;
    int range() const;
    int maxMana() const;
    int attackInterval() const;
    int moveInterval() const;
    int mana() const;
    UnitState state() const;
    const Placement& placement() const;

    bool isAlive() const;
    void setHp(int hp);
    void setMana(int mana);
    void setStar(int star);
    void setAcquireSeq(std::uint64_t acquireSeq);
    void setBaseStats(UnitStats stats);
    void setEffectiveStats(UnitStats stats);
    void setEquippedItemId(std::optional<ItemId> itemId);
    void setState(UnitState state);
    void setPlacement(Placement placement);

    virtual std::string archetype() const;
    virtual void castSkill(SkillContext& context);

private:
    std::string definitionId_;
    std::string name_;
    std::string visualKey_;
    std::string factoryKey_;
    Owner owner_;
    int hp_;
    int mana_ = 0;
    int star_ = 1;
    std::uint64_t acquireSeq_ = 0;
    UnitStats baseStats_;
    UnitStats effectiveStats_;
    std::optional<ItemId> equippedItemId_;
    std::vector<std::string> traits_;
    UnitState state_ = UnitState::Idle;
    Placement placement_ = Placement::none();
};

class BasicUnit : public Unit {
public:
    using Unit::Unit;

    std::string archetype() const override;
    void castSkill(SkillContext& context) override;
};

class Vanguard : public Unit {
public:
    using Unit::Unit;

    std::string archetype() const override;
    void castSkill(SkillContext& context) override;
};

class SparkMage : public Unit {
public:
    using Unit::Unit;

    std::string archetype() const override;
    void castSkill(SkillContext& context) override;
};

class IrisGuard : public Unit {
public:
    using Unit::Unit;

    std::string archetype() const override;
    void castSkill(SkillContext& context) override;
};

}  // namespace synera
