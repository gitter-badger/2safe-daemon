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
#include <QFileInfo>
#include <lib2safe/safeapi.h>

#include "safefilesystem.h"
#include "safecommon.h"

class SafeDaemon : public QObject {
    Q_OBJECT

public:
    SafeDaemon(QObject *parent = 0);
    bool isListening();
    QString socketPath();
    ~SafeDaemon();

private:
    SafeApi *api;
    QLocalServer *server;
    QSettings *settings;
    SafeFileSystem *filesystem;

    void authUser();
    void bindServer(QLocalServer *server, QString path);

    QJsonObject formSettingsReply(const QJsonArray &requestFields);
    QString getFilesystemPath();

private slots:
    void handleClientConnection();
    void authUserComplete();
    void fileAdded(const QFileInfo &info, const QString &hash, const uint &updatedAt);
    void fileChanged(const QFileInfo &info, const QString &hash, const uint &updatedAt);
};

#endif // SAFEDAEMON_H
