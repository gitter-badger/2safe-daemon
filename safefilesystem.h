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
    SafeFileSystem(const QString &path, const QString &dbName, QObject *parent);
    ~SafeFileSystem();

signals:
    void indexingStarted();
    void indexingFinished();
    void fileChanged(const QString &path);

private:
    bool debug;
    QString directory;
    QFileSystemWatcher watcher;
    QSqlDatabase database;
    void initDatabase(const QString &databaseName);
    void initWatcher(const QString &path);
    void reindexDirectory(const QString &path);
    void createDatabase();
    void saveFileInfo(const QString &path, const QString &hash, const uint &updatedAt);
    void updateFileInfo(const QString &path, const QString &hash, const uint &updatedAt);

public slots:
    void directoryChanged(const QString &path);
};

#endif // SAFEFILESYSTEM_H
