#pragma once

#include "Types.h"

#include <vector>

namespace synera {

class GameState;

class EncounterGenerator {
public:
    static std::vector<UnitId> generate(GameState& state, int round);
};

}  // namespace synera
