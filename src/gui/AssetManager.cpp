#include "AssetManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace synera::gui {

AssetManager::AssetManager(QString projectRoot) {
    // 资源查找顺序：可执行文件旁 assets、项目根 assets、当前工作目录 assets。
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
        // 加载失败的空 QPixmap 也缓存，避免每帧重复访问文件系统。
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
    // visualKey 可以带扩展名，也可以只写逻辑 key，常见图片扩展会自动补齐。
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
