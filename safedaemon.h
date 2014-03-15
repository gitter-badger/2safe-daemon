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
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QCryptographicHash>
#include <QSqlError>
#include <QMap>
#include <lib2safe/safeapi.h>

#include "safeapifactory.h"
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
    QSqlDatabase database;

    QMap<QString, SafeApi *> activeTransfers;
    void finishTransfer(const QString& path);

    void bindServer(QLocalServer *server, QString path);
    void initWatcher(const QString &path);
    void initDatabase(const QString &name);
    void createDatabase();
    void indexDirectory(const QString &path);

    QJsonObject formSettingsReply(const QJsonArray &requestFields);
    QString getFilesystemPath();
    bool isFileAllowed(const QFileInfo &info);
    QString makeHash(const QString &path);

private slots:
    void handleClientConnection();
    void fileAdded(const QString &path, bool isDir);
    void fileModified(const QString &hash);
    void insertFileInfo(const QString &path, const QString &hash, const uint &updatedAt);
    void updateFileInfo(const QString &path, const QString &hash, const uint &updatedAt);

    bool authUser();
    void deauthUser();
};

#endif // SAFEDAEMON_H
