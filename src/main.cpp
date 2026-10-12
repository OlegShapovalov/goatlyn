#include <QIcon>
#include <QApplication>
#include <string>
#include <QLoggingCategory>
#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    // Убираем артефакты дробного масштабирования Wayland
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--x11") {
            qputenv("QT_QPA_PLATFORM", "xcb");
        } else if (arg == "--wayland") {
            qputenv("QT_QPA_PLATFORM", "wayland;xcb");
        }
    }

    QApplication app(argc, argv);
    QLoggingCategory::setFilterRules("goatlyn.*.debug=false");
    app.setWindowIcon(QIcon(":/app_icon.png"));
    MainWindow win;
    win.show();
    return app.exec();
}