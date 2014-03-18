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

    if (!this->database.open()) {
        qWarning() << "Could not open database";
        return;
    }

    QString q("CREATE TABLE IF NOT EXISTS files ");
    q.append("(");
    q.append("_id INTEGER PRIMARY KEY,");
    q.append("id VARCHAR(32),");
    q.append("dir TEXT,");
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
}

SafeStateDb::~SafeStateDb()
{
    this->database.close();
}

void SafeStateDb::insertDir(QString path, QString name, ulong mtime,
                            QString id, QString hash)
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
    query.bindValue(":mtime", quint64(mtime));
    query.exec();
}

void SafeStateDb::insertFile(QString dir, QString path, QString name, ulong mtime,
                             QString hash, QString id)
{
    QSqlQuery query(this->database);
    QString q("INSERT OR REPLACE INTO files ");
    q.append("(id, dir, path, name, hash, mtime)");
    q.append(" VALUES ");
    q.append("(:id, :dir, :path, :name, :hash, :mtime)");
    query.prepare(q);
    query.bindValue(":id", id);
    query.bindValue(":dir", dir);
    query.bindValue(":path", path);
    query.bindValue(":name", name);
    query.bindValue(":hash", hash);
    query.bindValue(":mtime", quint64(mtime));
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

    if (query.exec()) {
        query.next();
        if(query.value(0).toInt() > 0) {
            return true;
        }
    }
    return false;
}

bool SafeStateDb::existsDir(QString path)
{
    QSqlQuery query(this->database);
    query.prepare("SELECT count(*) FROM dirs WHERE path=:path");
    query.bindValue(":path", path);
    if (query.exec() && query.next()) {
        if(query.value(0).toInt() > 0)
            return true;
    }
    return false;
}

void SafeStateDb::updateDirHash(QString dir)
{
    QSqlQuery query(this->database);
    query.prepare("SELECT hash FROM files WHERE dir=:dir");
    query.bindValue(":dir", dir);
    query.exec();
    QString hashstr;
    while(query.next()) {
        hashstr += query.value(0).toString();
    }
    QString hash(QCryptographicHash::hash(
                     hashstr.toUtf8(), QCryptographicHash::Md5).toHex());
    query.clear();
    query.prepare("UPDATE dirs SET hash=:hash WHERE path=:path");
    query.bindValue(":hash", hash);
    query.bindValue(":path", dir);
    query.exec();
}

void SafeStateDb::updateDirId(QString dir, QString dirId)
{
    QSqlQuery query(this->database);
    query.prepare("UPDATE dirs SET id=:id WHERE path=:path");
    query.bindValue(":id", dirId);
    query.bindValue(":path", dir);
    query.exec();
}

QString SafeStateDb::getDirId(QString path)
{
    QSqlQuery query(this->database);
    query.prepare("SELECT id FROM dirs WHERE path=:path");
    query.bindValue(":path", path);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return "";
}

QString SafeStateDb::getPathById(QString id)
{
    QSqlQuery query(this->database);
    query.prepare("SELECT path FROM dirs WHERE id=:id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return "";
}

QString SafeStateDb::findFile(QString hash)
{
    QSqlQuery query(this->database);
    query.prepare("SELECT path FROM files WHERE hash=:hash");
    query.bindValue(":hash", hash);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return "";
}

void SafeStateDb::query(const QString &str)
{
    QSqlQuery query(this->database);
    if(!query.prepare(str) || !query.exec()) {
        qWarning() << "Query is not valid:" << str;
    }
}

QString SafeStateDb::formPath(QString name)
{
    QString dbDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    return QDir(dbDir).filePath(name);
}
