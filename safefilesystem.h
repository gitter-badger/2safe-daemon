#ifndef SAFEFILESYSTEM_H
#define SAFEFILESYSTEM_H

#include <QObject>
#include <QString>
#include <QFileSystemWatcher>
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

class SafeFileSystem : public QObject {
    Q_OBJECT

public:
    SafeFileSystem(const QString &path, const QString &databaseName, QObject *parent);
    ~SafeFileSystem();
    void startWatching();

signals:
    void indexingStarted();
    void indexingFinished();
    void fileAdded(const QFileInfo &info, const QString &path, const uint &updatedAt);
    void fileChanged(const QFileInfo &info, const QString &path, const uint &updatedAt);

private:
    QString directory, databaseName;
    QFileSystemWatcher watcher;
    QSqlDatabase database;

    void initWatcher();
    void initDatabase();
    void createDatabase();
    void reindexDirectory(const QString &path);
    void saveFileInfo(const QString &path, const QString &hash, const uint &updatedAt);
    void updateFileInfo(const QString &path, const QString &hash, const uint &updatedAt);

public slots:
    void directoryChanged(const QString &path);
    void fileUploaded(const QFileInfo &info, const QString &hash, const uint &updatedAt);
    void newFileUploaded(const QFileInfo &info, const QString &hash, const uint &updatedAt);
};

#endif // SAFEFILESYSTEM_H
