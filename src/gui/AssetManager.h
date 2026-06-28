#pragma once

#include <QHash>
#include <QPixmap>
#include <QString>
#include <QStringList>

#include <string>

namespace synera::gui {

// 资源管理器按 visualKey 查找图片，并把加载后的 QPixmap 缓存在内存里。
class AssetManager {
public:
    explicit AssetManager(QString projectRoot = {});

    const QPixmap* pixmapFor(const std::string& visualKey);

private:
    QString findAssetPath(const QString& visualKey) const;

    QStringList searchRoots_;
    QHash<QString, QPixmap> cache_;
};

}  // namespace synera::gui
