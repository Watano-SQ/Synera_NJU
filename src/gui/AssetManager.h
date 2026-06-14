#pragma once

#include <QHash>
#include <QPixmap>
#include <QString>
#include <QStringList>

namespace synera::gui {

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
