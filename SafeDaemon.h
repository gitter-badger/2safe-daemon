#ifndef SAFEDAEMON_H
#define SAFEDAEMON_H

#include <QObject>
#include <QLocalServer>
#include <QSettings>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <QTextStream>
#include <lib2safe/safeapi.h>
#include "qtservice.h"
#include "safefilesystem.h"

class SafeDaemon : public QLocalServer {
    Q_OBJECT

public:
    SafeDaemon(const QString &name, QObject *parent);
    ~SafeDaemon();

private:
    SafeFileSystem *filesystem;
    QSettings *settings;
    SafeApi *api;
    void authUser();
    void incomingConnection(quintptr descriptor);
    void setSettings(const QJsonObject &requestArgs);
    QJsonObject getSettings(const QJsonArray &requestFields);

private slots:
    void readClient();
    void discardClient();
    void authUserComplete();
};

#endif // SAFEDAEMON_H
