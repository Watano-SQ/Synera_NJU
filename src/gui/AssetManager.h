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

    // 根据目录里的 visualKey 返回图片；找不到资源时返回 nullptr，由绘制层选择占位图或文字。
    // 返回指针指向内部缓存，调用方不能释放，也不应长期保存到 AssetManager 生命周期之外。
    const QPixmap* pixmapFor(const std::string& visualKey);

private:
    // 资源搜索顺序由构造函数准备好：可执行文件旁、项目根目录、当前工作目录等。
    // 这样开发环境、打包后的 release 目录和测试运行目录都能共用同一个查找函数。
    QString findAssetPath(const QString& visualKey) const;

    // searchRoots_ 保存候选 assets 根路径；cache_ 避免同一张图在每帧重读磁盘。
    QStringList searchRoots_;
    QHash<QString, QPixmap> cache_;
};

}  // namespace synera::gui
