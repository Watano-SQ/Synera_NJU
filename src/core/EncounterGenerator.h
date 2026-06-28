#pragma once

#include "Types.h"
#include "Unit.h"

#include <memory>
#include <string>
#include <vector>

namespace synera {

class GameState;

// 敌人生成计划先保存“单位对象 + 目标位置”，确认可生成后再真正写入 GameState。
struct EnemySpawnPlan {
    Position position;
    std::unique_ptr<Unit> unit;
};

// 每种敌人的基础模板，具体回合会在此基础上按轮数成长。
struct EnemyTemplate {
    std::string name;
    UnitStats stats;
    std::vector<std::string> traits;
    std::string visualKey;
    std::string factoryKey;
};

// PvE 波次生成器。它不保存状态，只根据当前棋盘和轮数生成敌人。
class EncounterGenerator {
public:
    static std::vector<EnemyTemplate> templatesForRound(int round);
    static std::vector<EnemySpawnPlan> plan(const GameState& state, int round, bool ignoreExistingEnemies = false);
    static std::vector<UnitId> generate(GameState& state, int round);
};

}  // namespace synera
