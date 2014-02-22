#include <QCoreApplication>
#include "safedaemon.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setOrganizationName(ORG_NAME);
    app.setApplicationName(APP_NAME);
    SafeDaemon daemon(&app);

    if (daemon.isListening()) {
        qDebug() << "Socket path: " << daemon.socketPath();
    } else {
        qDebug() << "Failed to start daemon. Now go fuck yourself.";
        return -1;
    }

    return app.exec();
}
