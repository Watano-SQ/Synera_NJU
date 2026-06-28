#include "Bench.h"

#include <stdexcept>

namespace synera {

Bench::Bench(std::size_t capacity) : slots_(capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("Bench capacity must be positive.");
    }
}

std::size_t Bench::capacity() const {
    return slots_.size();
}

bool Bench::isValidSlot(std::size_t slot) const {
    return slot < slots_.size();
}

bool Bench::isEmpty(std::size_t slot) const {
    return isValidSlot(slot) && !slots_[slot].has_value();
}

std::optional<std::size_t> Bench::firstEmptySlot() const {
    // 购买单位时总是优先填入最靠前的空槽，保证行为可预测。
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].has_value()) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<UnitId> Bench::occupant(std::size_t slot) const {
    if (!isValidSlot(slot)) {
        return std::nullopt;
    }
    return slots_[slot];
}

std::optional<UnitId> Bench::occupant(int slot) const {
    // GUI 和读档流程里常用 int，这里把负数安全地转换成“无单位”。
    if (slot < 0) {
        return std::nullopt;
    }
    return occupant(static_cast<std::size_t>(slot));
}

void Bench::setOccupant(std::size_t slot, UnitId id) {
    slots_[slot] = id;
}

void Bench::clear(std::size_t slot) {
    slots_[slot].reset();
}

}  // namespace synera
