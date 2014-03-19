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
#include <QMutex>
#include <lib2safe/safeapi.h>

#include "safeapifactory.h"
#include "safestatedb.h"
#include "fswatcher.h"
#include "safewatcher.h"
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
    SafeWatcher *swatcher;
    SafeStateDb *localStateDb;
    SafeStateDb *remoteStateDb;

    QMap<QString, QTimer *> pendingTransfers;
    QMap<QString, SafeApi *> activeTransfers;
    void finishTransfer(const QString& path);

    bool authUser();
    void bindServer(QLocalServer *server, QString path);
    void initWatcher(const QString &path);

    QJsonObject formSettingsReply(const QJsonArray &requestFields);
    QString getFilesystemPath();

    bool isFileAllowed(const QFileInfo &info);
    QString makeHash(const QFileInfo &info);
    QString makeHash(const QString &str);
    void updateDirHash(const QDir &dir);
    ulong getMtime(const QFileInfo &info);
    QString relativePath(const QFileInfo &info);
    QString relativeFilePath(const QFileInfo &info);
    QString fetchDirId(const QString &path);

    void fullRemoteIndex();
    void fullIndex(const QDir &dir);
    void checkIndex(const QDir &dir);

    QJsonObject fetchFileInfo(const QString &id);
    QJsonObject fetchDirInfo(const QString &id);

private slots:
    // FS handlers
    void fileAdded(const QString &path, bool isDir);
    void fileModified(const QString &path);
    void fileDeleted(const QString &path, bool isDir);
    void fileMoved(const QString &path1, const QString &path2);
    void fileCopied(const QString &path1, const QString &path2);

    // Remote handlers
    void remoteFileAdded(QString id, QString pid, QString name);
    void remoteFileDeleted(QString id, QString pid, QString name);
    void remoteDirectoryCreated(QString id, QString pid, QString name);
    void remoteDirectoryDeleted(QString id, QString pid, QString name);
    // from /d1/n1 to /d2/n2
    // pid1 = id of d1, pid2 = id of d2
    void remoteFileMoved(QString id, QString pid1, QString n1, QString pid2, QString n2);
    void remoteDirectoryMoved(QString id, QString pid1, QString n1, QString pid2, QString n2);

    // Instant actions
    QString createDir(const QString &parent_id, const QString &path);
    void remoteRemoveDir(const QFileInfo &info);
    void remoteRemoveFile(const QFileInfo &info);
    //void remoteCopyFile(const QString &path1, const QString &path2);
    //void remoteMoveFile(const QString &path1, const QString &path2);

    // Queued
    void queueUploadFile(const QString &dir_id, const QFileInfo &info);
    void uploadFile(const QString &dir_id, const QFileInfo &info);
    void queueDownloadFile(const QString &id, const QFileInfo &info);
    void downloadFile(const QString &id, const QFileInfo &info);

    // Misc
    void deauthUser();
    void purgeDb(const QString &name);
    void handleClientConnection();
};

#endif // SAFEDAEMON_H
