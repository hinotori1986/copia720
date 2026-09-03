/* main.cpp — punto de entrada de la interfaz gráfica de COPIA720. */
#include <QApplication>
#include "MainWindow.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("COPIA720"));
    app.setOrganizationName(QStringLiteral("AsturConsole"));

    MainWindow w;
    w.show();
    return app.exec();
}
