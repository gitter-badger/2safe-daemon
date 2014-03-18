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

    QMap<QString, QTimer *> pendingTransfers;
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
    ulong getMtime(const QFileInfo &info);
    QString relativePath(const QFileInfo &info);
    QString relativeFilePath(const QFileInfo &info);
    QString getDirId(const QString &path);

    void fullRemoteIndex();
    void fullIndex(const QDir &dir);
    void checkIndex(const QDir &dir);

private slots:
    void handleClientConnection();

    // FS handlers
    void fileAdded(const QString &path, bool isDir);
    void fileModified(const QString &path);
    void fileDeleted(const QString &path, bool isDir);
    void fileMoved(const QString &path1, const QString &path2);
    void fileCopied(const QString &path1, const QString &path2);

    QString createDir(const QString &parent_id, const QString &path);
    void removeDir(const QString &path);

    void queueUploadFile(const QString &dir_id, const QFileInfo &info);
    void uploadFile(const QString &dir_id, const QFileInfo &info);

    void removeFile(const QFileInfo &info);
    //void downloadFile(const QString &path);
    //void copyFile(const QString &path1, const QString &path2);
    //void moveFile(const QString &path1, const QString &path2);

    bool authUser();
    void deauthUser();
    void purgeDb(const QString &name);
};

#endif // SAFEDAEMON_H
