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
#include <QFile>
#include <QIODevice>
#include <QTextStream>
#include <QDir>
#include <QFileInfoList>
#include <QFileInfo>
#include <QDirIterator>
#include <QStandardPaths>
#include <QDateTime>
#include <QCryptographicHash>
#include <QMap>
#include <lib2safe/safeapi.h>

#include "safeapifactory.h"
#include "safestatedb.h"
#include "fswatcher.h"
#include "safecommon.h"

class SafeDaemon : public QObject {
    Q_OBJECT

public:
    SafeDaemon(QObject *parent = 0);
    ~SafeDaemon();
    bool isListening();
    QString socketPath();

private:
    SafeApiFactory *apiFactory;
    QLocalServer *server;
    QSettings *settings;
    FSWatcher *watcher;
    SafeStateDb *localStateDb;
    SafeStateDb *remoteStateDb;

    QMap<QString, SafeApi *> activeTransfers;
    void finishTransfer(const QString& path);

    void bindServer(QLocalServer *server, QString path);
    void initWatcher(const QString &path);

    QJsonObject formSettingsReply(const QJsonArray &requestFields);
    QString getFilesystemPath();

    bool isFileAllowed(const QFileInfo &info);
    QString makeHash(const QFileInfo &info);
    QString makeHash(const QString &str);
    uint getMtime(const QFileInfo &info);
    void fullIndex(const QDir &dir); // hash + mtime
    QMap<QString, uint> lightIndex(const QDir &dir); // mtime
    QString relativePath(const QFileInfo &info);
    QString relativeFilePath(const QFileInfo &info);

private slots:
    void handleClientConnection();
    void fileAdded(const QString &path, bool isDir);
    void fileModified(const QString &hash);

    bool authUser();
    void deauthUser();
    void purgeDb(const QString &name);
};

#endif // SAFEDAEMON_H
