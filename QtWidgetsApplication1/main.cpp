#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    try {
        MainWindow window;
        window.show();
        return app.exec();
    }
    catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Startup Error", QString("Failed to start: %1").arg(e.what()));
        return 1;
    }
    catch (...) {
        QMessageBox::critical(nullptr, "Startup Error", "Unknown error");
        return 1;
    }
}