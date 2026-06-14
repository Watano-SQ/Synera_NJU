#pragma once

#include "core/Types.h"

#include <QByteArray>

namespace synera::gui {

constexpr const char* kUnitDragMimeType = "application/x-synera-unit-drag";

enum class DragSourceType : qint32 {
    Bench = 0,
    Board = 1
};

struct UnitDragData {
    UnitId unitId = 0;
    DragSourceType sourceType = DragSourceType::Bench;
    int benchSlot = -1;
    Position boardPosition{-1, -1};
};

QByteArray encodeDragData(const UnitDragData& data);
bool decodeDragData(const QByteArray& bytes, UnitDragData& data);

}  // namespace synera::gui
