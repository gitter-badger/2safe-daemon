#ifndef SAFEDAEMON_H
#define SAFEDAEMON_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QFile>
#include <QIODevice>
#include <QTextStream>
#include <QLocalServer>
#include "qtservice.h"

class SafeDaemon : public QObject
{
    Q_OBJECT

public:
    SafeDaemon();

private:
    QFileSystemWatcher *watcher;
    QString logFilename;

public slots:
    void directoryChanged(const QString &path);
};

#endif // SAFEDAEMON_H
