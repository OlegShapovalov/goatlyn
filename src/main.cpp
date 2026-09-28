#include <QIcon>
#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/app_icon.png"));
    MainWindow win;
    win.show();
    return app.exec();
}
