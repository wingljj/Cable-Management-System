#include "MainWindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("设备工装管理系统"));
    app.setOrganizationName(QStringLiteral("DapmQt"));

    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(9);
    app.setFont(font);

    MainWindow window;
    window.show();
    return app.exec();
}
