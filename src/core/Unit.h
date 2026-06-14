#pragma once

#include "Types.h"

#include <string>
#include <utility>
#include <vector>

namespace synera {

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
    virtual ~Unit() = default;

    Unit(const Unit&) = delete;
    Unit& operator=(const Unit&) = delete;

    const std::string& name() const;
    const std::string& visualKey() const;
    Owner owner() const;
    const std::vector<std::string>& traits() const;

    int hp() const;
    int maxHp() const;
    int atk() const;
    int range() const;
    int maxMana() const;
    int mana() const;
    UnitState state() const;
    const Placement& placement() const;

    bool isAlive() const;
    void setHp(int hp);
    void setMana(int mana);
    void setState(UnitState state);
    void setPlacement(Placement placement);

    virtual std::string archetype() const;
    virtual void castSkill();

private:
    std::string name_;
    std::string visualKey_;
    Owner owner_;
    int hp_;
    int maxHp_;
    int atk_;
    int range_;
    int maxMana_;
    int mana_ = 0;
    std::vector<std::string> traits_;
    UnitState state_ = UnitState::Idle;
    Placement placement_ = Placement::none();
};

class BasicUnit : public Unit {
public:
    using Unit::Unit;

    std::string archetype() const override;
};

}  // namespace synera
