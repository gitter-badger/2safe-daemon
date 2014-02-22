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
//----
#include "safefilesystem.h"
#include "safecommon.h"

class SafeDaemon : public QObject {
    Q_OBJECT

public:
    SafeDaemon(QObject *parent = 0);
    bool isListening() { return server->isListening(); }
    QString socketPath() { return server->fullServerName(); }
    ~SafeDaemon();

private:
    SafeApi *api; // api facade
    QLocalServer *server; // socket listener
    QSettings *settings; // settings db
    SafeFileSystem *filesystem; // filesystem monitor

    // helpers
    void authUser();

    // message handlers
    QJsonObject formSettingsReply(const QJsonArray &requestFields);

    // utilities
    void bindServer(QLocalServer *server, QString path);
    QString getFilesystemPath();

private slots:
    void handleClientConnection();
    //
    void authUserComplete();
};

#endif // SAFEDAEMON_H
