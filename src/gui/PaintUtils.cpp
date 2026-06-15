#include "PaintUtils.h"

#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace synera::gui {

QRect aspectFitRect(QRect target, QSize sourceSize) {
    if (sourceSize.isEmpty() || target.isEmpty()) {
        return {};
    }

    const QSize scaled = sourceSize.scaled(target.size(), Qt::KeepAspectRatio);
    return QRect(QPoint(target.center().x() - scaled.width() / 2,
                       target.center().y() - scaled.height() / 2),
                 scaled);
}

void drawPixmapAspectFit(QPainter& painter, QRect target, const QPixmap& pixmap) {
    if (pixmap.isNull() || target.isEmpty()) {
        return;
    }

    painter.drawPixmap(aspectFitRect(target, pixmap.size()), pixmap);
}

void drawPixmapCroppedToFill(QPainter& painter, QRect target, const QPixmap& pixmap) {
    if (pixmap.isNull() || target.isEmpty()) {
        return;
    }

    QRect source(0, 0, pixmap.width(), pixmap.height());
    const double sourceRatio = static_cast<double>(source.width()) / std::max(1, source.height());
    const double targetRatio = static_cast<double>(target.width()) / std::max(1, target.height());

    if (sourceRatio > targetRatio) {
        const int width = static_cast<int>(source.height() * targetRatio);
        source.setLeft((source.width() - width) / 2);
        source.setWidth(width);
    } else if (sourceRatio < targetRatio) {
        const int height = static_cast<int>(source.width() / targetRatio);
        source.setTop((source.height() - height) / 2);
        source.setHeight(height);
    }

    painter.drawPixmap(target, pixmap, source);
}

}  // namespace synera::gui
