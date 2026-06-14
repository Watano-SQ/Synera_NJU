#include "Player.h"

#include <algorithm>

namespace synera {

Player::Player(int hp, int gold, int level, int unitCap)
    : hp_(hp), gold_(gold), level_(level), unitCap_(unitCap) {}

int Player::hp() const {
    return hp_;
}

int Player::gold() const {
    return gold_;
}

int Player::level() const {
    return level_;
}

int Player::unitCap() const {
    return unitCap_;
}

int Player::currentRound() const {
    return currentRound_;
}

void Player::setHp(int hp) {
    hp_ = std::max(0, hp);
}

void Player::setGold(int gold) {
    gold_ = std::max(0, gold);
}

void Player::setLevel(int level) {
    level_ = std::max(1, level);
}

void Player::setUnitCap(int unitCap) {
    unitCap_ = std::max(1, unitCap);
}

void Player::setCurrentRound(int round) {
    currentRound_ = std::max(1, round);
}

}  // namespace synera
