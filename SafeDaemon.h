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

#include "safeapifactory.h"
#include "safefilesystem.h"
#include "safecommon.h"

class SafeDaemon : public QObject {
    Q_OBJECT

public:
    SafeDaemon(QObject *parent = 0);
    bool isListening();
    QString socketPath();
    ~SafeDaemon();

signals:
    void fileUploaded(const QFileInfo &info, const QString &hash, const uint &updatedAt);
    void newFileUploaded(const QFileInfo &info, const QString &hash, const uint &updatedAt);

private:
    SafeApiFactory *apiFactory;
    QLocalServer *server;
    QSettings *settings;
    SafeFileSystem *filesystem;

    void bindServer(QLocalServer *server, QString path);

    QJsonObject formSettingsReply(const QJsonArray &requestFields);
    QString getFilesystemPath();

private slots:
    void handleClientConnection();
    void fileAdded(const QFileInfo &info, const QString &hash, const uint &updatedAt);
    void fileChanged(const QFileInfo &info, const QString &hash, const uint &updatedAt);
};

#endif // SAFEDAEMON_H
