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
#include <QEventLoop>
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
    QString updateDirHash(const QDir &dir);
    uint getMtime(const QFileInfo &info);
    void fullIndex(const QDir &dir); // hash + mtime
    void checkIndex(const QDir &dir); // mtime
    QString relativePath(const QFileInfo &info);
    QString relativeFilePath(const QFileInfo &info);
    QString getDirId(const QString &path);

private slots:
    void handleClientConnection();

    // FS handlers
    void fileAdded(const QString &path, bool isDir);
    void fileModified(const QString &path);
    void fileDeleted(const QString &path);
    void fileMoved(const QString &path1, const QString &path2);
    void fileCopied(const QString &path1, const QString &path2);

    // TODO: file actions queues

    /* WIP
    void createDir();
    void removeDir();
    void uploadFile();
    void downloadFile;
    void removeFile();
    void copyFile();
    void moveFile();
    */

    bool authUser();
    void deauthUser();
    void purgeDb(const QString &name);
};

#endif // SAFEDAEMON_H
