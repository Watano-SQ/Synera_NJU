#pragma once

#include <QRect>
#include <QSize>

class QPainter;
class QPixmap;

namespace synera::gui {

QRect aspectFitRect(QRect target, QSize sourceSize);
void drawPixmapAspectFit(QPainter& painter, QRect target, const QPixmap& pixmap);
void drawPixmapCroppedToFill(QPainter& painter, QRect target, const QPixmap& pixmap);

}  // namespace synera::gui
