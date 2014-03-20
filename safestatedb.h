#ifndef SAFESTATEDB_H
#define SAFESTATEDB_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QCryptographicHash>

class SafeStateDb : public QObject
{
    Q_OBJECT
public:
    explicit SafeStateDb(QString name, QObject *parent = 0);
    ~SafeStateDb();
    void insertDir(QString path, QString name, ulong mtime, QString id = QString(),
                   QString hash = QString());
    void insertFile(QString dir, QString path, QString name, ulong mtime,
                    QString hash = QString(), QString id = QString());
    void removeDir(QString path);
    void removeDirRecursively(QString path);
    void removeFile(QString path);
    bool existsFile(QString path);
    bool existsDir(QString path);
    QString findFile(QString hash);
    void updateDirHash(QString dir);
    void updateDirId(QString dir, QString dirId);
    QString getFileId(QString path);
    QString getDirId(QString path);
    ulong getFileMtime(QString path);

    QString getDirPathById(QString id);
    ulong getFileMtimeById(QString id);
    QString getFileHashById(QString id);
    void removeFileById(QString id);
    void removeDirById(QString id);
    void removeDirByIdRecursively(QString id);
    bool existsFileById(QString id);
    bool existsDirById(QString id);

    static QString formPath(QString name);

signals:


private:
    QSqlDatabase database;
    void query(const QString &str);
};

#endif // SAFESTATEDB_H
