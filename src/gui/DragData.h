#pragma once

#include "core/Types.h"

#include <QByteArray>

namespace synera::gui {

// Qt 拖拽使用的自定义 MIME 类型，棋盘和 Bench 都靠它识别单位拖拽。
constexpr const char* kUnitDragMimeType = "application/x-synera-unit-drag";

// 记录拖拽来源，落点控制器会根据来源决定是部署、移动还是回 Bench。
enum class DragSourceType : qint32 {
    Bench = 0,
    Board = 1
};

// 一次单位拖拽携带的最小信息：单位 ID、来源类型和原位置。
struct UnitDragData {
    UnitId unitId = 0;
    DragSourceType sourceType = DragSourceType::Bench;
    int benchSlot = -1;
    Position boardPosition{-1, -1};
};

// 拖拽数据需要序列化到 QByteArray 才能放进 QMimeData。
QByteArray encodeDragData(const UnitDragData& data);
bool decodeDragData(const QByteArray& bytes, UnitDragData& data);

}  // namespace synera::gui
