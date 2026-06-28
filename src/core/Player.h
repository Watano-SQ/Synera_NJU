#pragma once

namespace synera {

// Player 保存玩家级别的资源和进度。具体规则如升级花费、胜败奖励由 GameState 决定。
class Player {
public:
    Player(int hp = 100, int gold = 0, int level = 1, int unitCap = 3);

    int hp() const;
    int gold() const;
    int level() const;
    int unitCap() const;
    int currentRound() const;

    void setHp(int hp);
    void setGold(int gold);
    void setLevel(int level);
    void setUnitCap(int unitCap);
    void setCurrentRound(int round);

private:
    int hp_;
    int gold_;
    int level_;
    int unitCap_;
    int currentRound_ = 1;
};

}  // namespace synera
