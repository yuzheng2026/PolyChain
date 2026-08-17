#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 读取命令行参数：第一个参数为监听端口（默认 12345）
    quint16 port = 12345;
    if (argc > 1) {
        bool ok;
        int p = QString(argv[1]).toInt(&ok);
        if (ok && p > 0 && p < 65536)
            port = static_cast<quint16>(p);
    }

    try {
        MainWindow w(port);
        w.show();
        return app.exec();
    }
    catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Startup Error", e.what());
        return 1;
    }
    catch (...) {
        QMessageBox::critical(nullptr, "Startup Error", "Unknown error");
        return 1;
    }
}
