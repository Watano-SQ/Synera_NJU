#include "Unit.h"

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
    : name_(std::move(name)),
      visualKey_(std::move(visualKey)),
      owner_(owner),
      hp_(maxHp),
      maxHp_(maxHp),
      atk_(atk),
      range_(range),
      maxMana_(maxMana),
      traits_(std::move(traits)) {}

const std::string& Unit::name() const {
    return name_;
}

const std::string& Unit::visualKey() const {
    return visualKey_;
}

Owner Unit::owner() const {
    return owner_;
}

const std::vector<std::string>& Unit::traits() const {
    return traits_;
}

int Unit::hp() const {
    return hp_;
}

int Unit::maxHp() const {
    return maxHp_;
}

int Unit::atk() const {
    return atk_;
}

int Unit::range() const {
    return range_;
}

int Unit::maxMana() const {
    return maxMana_;
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
    hp_ = std::clamp(hp, 0, maxHp_);
    if (hp_ == 0) {
        state_ = UnitState::Dead;
    }
}

void Unit::setMana(int mana) {
    mana_ = std::clamp(mana, 0, maxMana_);
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

void Unit::castSkill() {}

std::string BasicUnit::archetype() const {
    return "BasicUnit";
}

}  // namespace synera
