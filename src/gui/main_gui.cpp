#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    synera::gui::MainWindow window;
    window.show();
    return app.exec();
}
