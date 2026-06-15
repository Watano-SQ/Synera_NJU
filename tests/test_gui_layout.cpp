#include "gui/AssetManager.h"
#include "gui/BenchWidget.h"
#include "gui/BoardWidget.h"
#include "gui/EquipmentPanel.h"
#include "gui/MainWindow.h"
#include "gui/ShopPanel.h"
#include "core/GameState.h"

#include <QApplication>
#include <QDir>
#include <QPixmap>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QWidget>

#include <iostream>

using namespace synera;
using namespace synera::gui;

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    auto expect = [](bool condition, const char* message) {
        if (!condition) {
            std::cerr << "layout check failed: " << message << '\n';
            return false;
        }
        return true;
    };

    GameState game;
    AssetManager assets(QString::fromUtf8(SYNERA_PROJECT_ROOT));

    BoardWidget board(&game, &assets);
    if (!expect(board.sizeHint() == QSize(512, 512), "board sizeHint is 512x512") ||
        !expect(board.minimumSize() == QSize(512, 512), "board minimum is 512x512") ||
        !expect(board.maximumSize() == QSize(512, 512), "board maximum is 512x512") ||
        !expect(board.sizePolicy().horizontalPolicy() == QSizePolicy::Fixed, "board horizontal policy is fixed") ||
        !expect(board.sizePolicy().verticalPolicy() == QSizePolicy::Fixed, "board vertical policy is fixed")) {
        return 1;
    }

    BenchWidget bench(&game, &assets);
    if (!expect(bench.sizeHint().width() == 512, "bench sizeHint width is 512") ||
        !expect(bench.sizeHint().height() == 96, "bench sizeHint height is 96") ||
        !expect(bench.minimumHeight() == 96, "bench minimum height is 96") ||
        !expect(bench.maximumHeight() == 96, "bench maximum height is 96") ||
        !expect(bench.sizePolicy().horizontalPolicy() == QSizePolicy::Fixed, "bench horizontal policy is fixed") ||
        !expect(bench.sizePolicy().verticalPolicy() == QSizePolicy::Fixed, "bench vertical policy is fixed")) {
        return 1;
    }

    ShopPanel shop(&game, &assets);
    if (!expect(shop.maximumHeight() <= 210, "shop maximum height is under 210") ||
        !expect(shop.minimumWidth() >= 780, "shop minimum width supports merged shop container") ||
        !expect(shop.sizePolicy().verticalPolicy() != QSizePolicy::Expanding, "shop vertical policy is not expanding")) {
        return 1;
    }

    EquipmentPanel equipment(&game, &assets);
    if (!expect(equipment.maximumHeight() <= 130, "equipment maximum height is under 130") ||
        !expect(equipment.minimumWidth() >= 660, "equipment minimum width fits the tray") ||
        !expect(equipment.sizePolicy().verticalPolicy() != QSizePolicy::Expanding,
                "equipment vertical policy is not expanding")) {
        return 1;
    }
    QWidget* tray = equipment.findChild<QWidget*>("equipmentTray");
    if (!expect(tray != nullptr, "equipment tray exists") ||
        !expect(tray->sizeHint().width() <= 780, "equipment tray width is inside right column") ||
        !expect(tray->sizeHint().height() <= 92, "equipment tray height leaves room for title") ||
        !expect(tray->sizePolicy().horizontalPolicy() == QSizePolicy::Fixed, "equipment tray horizontal policy is fixed") ||
        !expect(tray->sizePolicy().verticalPolicy() == QSizePolicy::Fixed, "equipment tray vertical policy is fixed")) {
        return 1;
    }

    MainWindow window;
    if (!expect(window.minimumSize() == QSize(1360, 760), "main window minimum is 1360x760")) {
        return 1;
    }
    window.show();
    app.processEvents();
    QDir(QString::fromUtf8(SYNERA_PROJECT_ROOT)).mkpath("artifacts");
    const QString screenshotPath =
        QString::fromUtf8(SYNERA_PROJECT_ROOT) + "/artifacts/gui_layout_grab.png";
    if (!expect(window.grab().save(screenshotPath), "main window grab screenshot saved")) {
        return 1;
    }

    return 0;
}
