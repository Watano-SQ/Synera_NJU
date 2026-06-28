#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    // Qt GUI 的正式入口：创建 QApplication 后展示 MainWindow。
    QApplication app(argc, argv);
    synera::gui::MainWindow window;
    window.show();
    return app.exec();
}
