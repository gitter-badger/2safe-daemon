#include "safedaemon.h"

SafeDaemon::SafeDaemon() : QObject()
{
    this->logFilename = "/Users/awolf/2safe.log"; // TODO use some kind of preferences

    this->watcher = new QFileSystemWatcher(); // TODO should we delete it somewhere?
    this->watcher->addPath("/Users/awolf/2safe/"); // TODO use some kind of preferences

    connect(this->watcher, &QFileSystemWatcher::directoryChanged, this, &SafeDaemon::directoryChanged);
}

void SafeDaemon::directoryChanged(const QString &path)
{
    // TODO add mutex?
    QFile logFile(this->logFilename);

    if (logFile.open(QIODevice::WriteOnly)) {
        QTextStream logStream(&logFile);
        logStream << path << "\n";
        logFile.close();
    }
}
