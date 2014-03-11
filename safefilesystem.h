#ifndef SAFEFILESYSTEM_H
#define SAFEFILESYSTEM_H

#include <QObject>
#include <QString>
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
#include <QDebug>

#include "fswatcher.h"

class SafeFileSystem : public QObject {
    Q_OBJECT

public:
    SafeFileSystem(const QString &path, const QString &databaseName, QObject *parent);
    void startWatching();

signals:
    void indexingStarted();
    void indexingFinished();
    void fileAddedSignal(const QFileInfo &info, const QString &path, const uint &updatedAt);
    void fileModifiedSignal(const QFileInfo &info, const QString &path, const uint &updatedAt);

private:
    QString directory, databaseName;
    FSWatcher *watcher;
    QSqlDatabase database;

    void initWatcher();
    void initDatabase();
    void createDatabase();
    void reindexDirectory(const QString &path);
    void saveFileInfo(const QString &path, const QString &hash, const uint &updatedAt);
    void updateFileInfo(const QString &path, const QString &hash, const uint &updatedAt);

public slots:
    void fileUploaded(const QFileInfo &info, const QString &hash, const uint &updatedAt);
    void newFileUploaded(const QFileInfo &info, const QString &hash, const uint &updatedAt);

    void fileAdded(const QString &path);
    void fileModified(const QString &path);
};

#endif // SAFEFILESYSTEM_H
