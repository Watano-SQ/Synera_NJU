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
    // unitId 是拖拽的核心身份。落点控件不会信任可视元素本身，而是回到 GameState 再查一次单位。
    UnitId unitId = 0;
    // sourceType 决定落点动作：Bench -> Board 是部署，Board -> Board 是移动/交换，Board -> Bench 是撤回。
    DragSourceType sourceType = DragSourceType::Bench;
    // 当 sourceType == Bench 时使用；其它来源保留 -1 作为无效值，便于做防御性检查。
    int benchSlot = -1;
    // 当 sourceType == Board 时使用；Bench 来源保留 (-1, -1)，避免误把旧坐标当作真实来源。
    Position boardPosition{-1, -1};
};

// 拖拽数据需要序列化到 QByteArray 才能放进 QMimeData。
QByteArray encodeDragData(const UnitDragData& data);
bool decodeDragData(const QByteArray& bytes, UnitDragData& data);

}  // namespace synera::gui
