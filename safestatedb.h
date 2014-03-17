#ifndef SAFESTATEDB_H
#define SAFESTATEDB_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

class SafeStateDb : public QObject
{
    Q_OBJECT
public:
    explicit SafeStateDb(QString name, QObject *parent = 0);
    void insertDir(QString id, QString path, QString name, QString hash, uint mtime);
    void insertFile(QString dir, QString path, QString name, QString hash, uint mtime);
    void removeDir(QString path);
    void removeFile(QString path);
    bool existsFile(QString path);
    bool existsDir(QString path);
    QString findFile(QString hash);
    bool open();

    static QString formPath(QString name);

signals:

public slots:
    void close();

private:
    QSqlDatabase database;
    void query(const QString &str);
};

#endif // SAFESTATEDB_H
