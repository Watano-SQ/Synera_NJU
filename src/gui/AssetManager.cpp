#include "AssetManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace synera::gui {

AssetManager::AssetManager(QString projectRoot) {
    const QString appAssets = QDir(QCoreApplication::applicationDirPath()).filePath("assets");
    if (QDir(appAssets).exists()) {
        searchRoots_.push_back(appAssets);
    }

    if (!projectRoot.isEmpty()) {
        const QString projectAssets = QDir(projectRoot).filePath("assets");
        if (QDir(projectAssets).exists() && !searchRoots_.contains(projectAssets)) {
            searchRoots_.push_back(projectAssets);
        }
    }

    const QString cwdAssets = QDir(QDir::currentPath()).filePath("assets");
    if (QDir(cwdAssets).exists() && !searchRoots_.contains(cwdAssets)) {
        searchRoots_.push_back(cwdAssets);
    }
}

const QPixmap* AssetManager::pixmapFor(const std::string& visualKey) {
    if (visualKey.empty()) {
        return nullptr;
    }

    const QString key = QString::fromStdString(visualKey);
    if (cache_.contains(key)) {
        const QPixmap& cached = cache_[key];
        return cached.isNull() ? nullptr : &cached;
    }

    QPixmap pixmap;
    const QString path = findAssetPath(key);
    if (!path.isEmpty()) {
        pixmap.load(path);
    }
    cache_.insert(key, pixmap);
    const QPixmap& cached = cache_[key];
    return cached.isNull() ? nullptr : &cached;
}

QString AssetManager::findAssetPath(const QString& visualKey) const {
    const QStringList candidates{
        visualKey,
        visualKey + ".png",
        visualKey + ".jpg",
        visualKey + ".jpeg",
    };

    for (const QString& root : searchRoots_) {
        for (const QString& candidate : candidates) {
            const QString path = QDir(root).filePath(candidate);
            if (QFileInfo::exists(path) && QFileInfo(path).isFile()) {
                return path;
            }
        }
    }
    return {};
}

}  // namespace synera::gui
