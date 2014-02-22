#include "safeservice.h"

SafeService::SafeService(int argc, char **argv) : QtService<QCoreApplication>(argc, argv, "2 Safe Daemon") {
    setServiceDescription("2Safe Daemon");
    //QCoreApplication::setOrganizationName("2safe");
    //QCoreApplication::setApplicationName("2safe");
}

void SafeService::start() {
    QCoreApplication *app = application();

    QString socketPath = QDir::cleanPath(QDir::homePath() + "/.2safe/control.sock");
    this->daemon = new SafeDaemon(socketPath, app);

    if (this->daemon->isListening()) {
        qDebug() << "Socket path: " << this->daemon->fullServerName();
    } else {
        qDebug() << "Failed to start daemon. Now go fuck yourself.";
    }
}

void SafeService::stop() {

}
