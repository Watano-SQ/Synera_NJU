#pragma once

#include <QRect>
#include <QSize>

class QPainter;
class QPixmap;

namespace synera::gui {

// 常用图片绘制工具：等比完整显示或裁剪填充，避免各控件重复写缩放逻辑。
QRect aspectFitRect(QRect target, QSize sourceSize);
void drawPixmapAspectFit(QPainter& painter, QRect target, const QPixmap& pixmap);
void drawPixmapCroppedToFill(QPainter& painter, QRect target, const QPixmap& pixmap);

}  // namespace synera::gui
