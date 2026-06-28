#include "DragData.h"

#include <QDataStream>
#include <QIODevice>

namespace synera::gui {

QByteArray encodeDragData(const UnitDragData& data) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    // 按固定顺序写入，decodeDragData 必须用同样顺序读取。
    stream << static_cast<qulonglong>(data.unitId);
    stream << static_cast<qint32>(data.sourceType);
    stream << static_cast<qint32>(data.benchSlot);
    stream << static_cast<qint32>(data.boardPosition.row);
    stream << static_cast<qint32>(data.boardPosition.col);
    return bytes;
}

bool decodeDragData(const QByteArray& bytes, UnitDragData& data) {
    QDataStream stream(bytes);
    qulonglong unitId = 0;
    qint32 sourceType = 0;
    qint32 benchSlot = -1;
    qint32 row = -1;
    qint32 col = -1;

    stream >> unitId >> sourceType >> benchSlot >> row >> col;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    // 只接受已知来源，防止其他拖拽数据被误当成单位拖拽。
    if (sourceType != static_cast<qint32>(DragSourceType::Bench) &&
        sourceType != static_cast<qint32>(DragSourceType::Board)) {
        return false;
    }

    data.unitId = static_cast<UnitId>(unitId);
    data.sourceType = static_cast<DragSourceType>(sourceType);
    data.benchSlot = benchSlot;
    data.boardPosition = Position{row, col};
    return true;
}

}  // namespace synera::gui
