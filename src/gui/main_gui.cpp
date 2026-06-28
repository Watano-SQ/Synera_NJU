#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    // Qt GUI 的正式入口。
    // QApplication 必须先于任何 QWidget 创建，因为它负责事件循环、字体、平台插件和全局 UI 资源。
    // MainWindow 内部再持有 GameState，并把所有按钮、拖拽、计时器事件转发到核心规则层。
    QApplication app(argc, argv);
    synera::gui::MainWindow window;
    // show() 只安排窗口显示；真正的消息泵从 app.exec() 开始运行。
    window.show();
    return app.exec();
}
