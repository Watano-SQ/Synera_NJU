#pragma once

#include "Types.h"

#include <optional>
#include <vector>

namespace synera {

class GameState;

class Bench {
public:
    explicit Bench(std::size_t capacity = 8);

    std::size_t capacity() const;
    bool isValidSlot(std::size_t slot) const;
    bool isEmpty(std::size_t slot) const;
    std::optional<std::size_t> firstEmptySlot() const;
    std::optional<UnitId> occupant(std::size_t slot) const;
    std::optional<UnitId> occupant(int slot) const;

private:
    friend class GameState;

    void setOccupant(std::size_t slot, UnitId id);
    void clear(std::size_t slot);

    std::vector<std::optional<UnitId>> slots_;
};

}  // namespace synera
