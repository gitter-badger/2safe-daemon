#include "safestatedb.h"

SafeStateDb::SafeStateDb(QString name, QObject *parent) :
    QObject(parent)
{
    QString dbDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    if (dbDir.isEmpty()) {
        qWarning() << "Can not find database location";
        return;
    }
    if (!QDir(dbDir).exists()) {
        QDir().mkpath(dbDir);
    }

    QString dbPath = QDir(dbDir).filePath(name);
    qDebug() << "Using database path:" << dbPath;

    this->database = QSqlDatabase::addDatabase("QSQLITE", name + QString("_conn"));
    this->database.setDatabaseName(dbPath);
    if(!open())
        return;

    QString q("CREATE TABLE IF NOT EXISTS files ");
    q.append("(");
    q.append("_id INTEGER PRIMARY KEY,");
    q.append("path TEXT,");
    q.append("name VARCHAR(255),");
    q.append("hash VARCHAR(32),");
    q.append("mtime INTEGER");
    q.append(")");
    query(q);

    q = "CREATE TABLE IF NOT EXISTS dirs ";
    q.append("(");
    q.append("_id INTEGER PRIMARY KEY,");
    q.append("id VARCHAR(32),");
    q.append("path TEXT,");
    q.append("name VARCHAR(255),");
    q.append("hash VARCHAR(32),");
    q.append("mtime INTEGER");
    q.append(")");
    query(q);

    close();
}

void SafeStateDb::insertDir(QString id, QString path, QString name, QString hash, uint mtime)
{
    QSqlQuery query(this->database);
    QString q("INSERT OR REPLACE INTO dirs ");
    q.append("(id, path, name, hash, mtime)");
    q.append(" VALUES ");
    q.append("(:id, :path, :name, :hash, :mtime)");
    query.prepare(q);
    query.bindValue(":id", id);
    query.bindValue(":path", path);
    query.bindValue(":name", name);
    query.bindValue(":hash", hash);
    query.bindValue(":mtime", mtime);
    query.exec();
}

void SafeStateDb::insertFile(QString dir, QString path, QString name, QString hash, uint mtime)
{
    QSqlQuery query(this->database);
    QString q("INSERT OR REPLACE INTO files ");
    q.append("(dir, path, name, hash, mtime)");
    q.append(" VALUES ");
    q.append("(:dir, :path, :name, :hash, :mtime)");
    query.prepare(q);
    query.bindValue(":dir", dir);
    query.bindValue(":path", path);
    query.bindValue(":name", name);
    query.bindValue(":hash", hash);
    query.bindValue(":mtime", mtime);
    query.exec();
}

void SafeStateDb::removeDir(QString path)
{
    QSqlQuery query(this->database);
    query.prepare("DELETE FROM dirs WHERE path=:path");
    query.bindValue(":path", path);
    query.exec();
}

void SafeStateDb::removeFile(QString path)
{
    QSqlQuery query(this->database);
    query.prepare("DELETE FROM files WHERE path=:path");
    query.bindValue(":path", path);
    query.exec();
}

bool SafeStateDb::existsFile(QString path)
{
    QSqlQuery query(this->database);
    query.prepare("SELECT count(*) FROM files WHERE path=:path");
    query.bindValue(":path", path);
    query.exec();
    if(query.value(0).toInt() > 0) {
        return true;
    }
    return false;
}

bool SafeStateDb::existsDir(QString path)
{
    QSqlQuery query(this->database);
    query.prepare("SELECT count(*) FROM dirs WHERE path=:path");
    query.bindValue(":path", path);
    query.exec();
    if(query.value(0).toInt() > 0) {
        return true;
    }
    return false;
}

QString SafeStateDb::findFile(QString hash)
{
    QSqlQuery query(this->database);
    query.prepare("SELECT path FROM files WHERE hash=:hash");
    query.bindValue(":hash", hash);
    query.exec();
    return query.value(0).toString();
}

void SafeStateDb::query(const QString &str)
{
    QSqlQuery query(this->database);
    if(!query.prepare(str) || !query.exec()) {
        qWarning() << "Query is not valid:" << str;
    }
}

bool SafeStateDb::open()
{
    if (!this->database.open()) {
        qWarning() << "Can not open database";
        return false;
    }
    return true;
}

void SafeStateDb::close()
{
    this->database.close();
}


QString SafeStateDb::formPath(QString name)
{
    QString dbDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    return QDir(dbDir).filePath(name);
}
