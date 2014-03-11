#include "safefilesystem.h"

SafeFileSystem::SafeFileSystem(const QString &path, const QString &databaseName, QObject *parent = 0) : QObject(parent) {
    this->directory = path;
    this->databaseName = databaseName;
}

void SafeFileSystem::startWatching() {
    this->initDatabase();
    this->createDatabase();
    this->initWatcher();
}

void SafeFileSystem::initDatabase() {
    QString databaseDirectory = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    if (databaseDirectory.isEmpty()) {
        qDebug() << "Can not find database location";
    } else {
        if (!QDir(databaseDirectory).exists()) {
            QDir().mkpath(databaseDirectory);
        }

        QString databasePath = QDir(databaseDirectory).filePath(this->databaseName);
        qDebug() << "Using database path:" << databasePath;

        this->database = QSqlDatabase::addDatabase("QSQLITE");
        this->database.setDatabaseName(databasePath);

        if (!this->database.open()) {
            qDebug() << "Can not open database";
        }
    }
}

void SafeFileSystem::initWatcher() {
    this->watcher = new FSWatcher(this->directory, this);
    connect(this->watcher, &FSWatcher::added, this, &SafeFileSystem::fileAdded);
    //connect(this->watcher, &FSWatcher::modified, this, &SafeFileSystem::fileModified);
    this->watcher->watch();

    QDirIterator iterator(this->directory, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    this->reindexDirectory(this->directory);
    while (iterator.hasNext()) {
        this->reindexDirectory(iterator.next());
    }
}

void SafeFileSystem::reindexDirectory(const QString &path) {
    QDirIterator iterator(path, QDir::Files);

    while (iterator.hasNext()) {
        iterator.next();

        QFileInfo info = iterator.fileInfo();
        QDateTime updatedAtFs = info.lastModified();

        QSqlQuery selectQuery(this->database);
        QString hash(QCryptographicHash::hash(iterator.filePath().toUtf8(), QCryptographicHash::Md5).toHex());
        selectQuery.prepare("SELECT * FROM files WHERE hash = :hash");
        selectQuery.bindValue(":hash", hash);

        if (!selectQuery.exec()) {
            qDebug() << "Can not run database query";
        } else {
            QSqlRecord record = selectQuery.record();

            if (selectQuery.next()) {
                uint updatedAtDb = selectQuery.value(record.indexOf("updated_at")).toUInt();

                if (updatedAtFs.toTime_t() != updatedAtDb) {
                    qDebug() << "File modified:" << iterator.filePath();
                    emit this->fileModifiedSignal(iterator.fileInfo(), hash, updatedAtFs.toTime_t());
                }
            } else {
                qDebug() << "File added:" << iterator.filePath();
                emit this->fileAddedSignal(iterator.fileInfo(), hash, updatedAtFs.toTime_t());
            }
        }
    }
}

void SafeFileSystem::saveFileInfo(const QString &path, const QString &hash, const uint &updatedAt) {
    QSqlQuery query(this->database);
    query.prepare("INSERT INTO files (hash, path, updated_at) VALUES (:hash, :path, :updated_at)");
    query.bindValue(":hash", hash);
    query.bindValue(":path", path);
    query.bindValue(":updated_at", updatedAt);

    if (!query.exec()) {
        qDebug() << "Can not run database query:" << query.lastError().text();
    }
}

void SafeFileSystem::updateFileInfo(const QString &path, const QString &hash, const uint &updatedAt) {
    QSqlQuery query(this->database);
    query.prepare("UPDATE files SET path = :path, updated_at = :updated_at WHERE hash = :hash");
    query.bindValue(":hash", hash);
    query.bindValue(":path", path);
    query.bindValue(":updated_at", updatedAt);

    if (!query.exec()) {
        qDebug() << "Can not run database query:" << query.lastError().text();
    }
}

void SafeFileSystem::createDatabase() {
    QSqlQuery query(this->database);
    query.prepare("CREATE TABLE IF NOT EXISTS files (hash VARCHAR(32) PRIMARY KEY, path TEXT, updated_at INTEGER)");

    if (!query.exec()) {
        qDebug() << "Can not run database query";
    }
}

void SafeFileSystem::newFileUploaded(const QFileInfo &info, const QString &hash, const uint &updatedAt) {
    qDebug() << "New file info saved";

    this->saveFileInfo(info.filePath(), hash, updatedAt);
}

void SafeFileSystem::fileUploaded(const QFileInfo &info, const QString &hash, const uint &updatedAt) {
    qDebug() << "File info updated";

    this->updateFileInfo(info.filePath(), hash, updatedAt);
}

void SafeFileSystem::fileAdded(const QString &path) {
    qDebug() << "File added: " << path;

    QFileInfo info(path);
    QString hash(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex());
    emit this->fileAddedSignal(info, hash, info.lastModified().toTime_t());
}

void SafeFileSystem::fileModified(const QString &path) {
    qDebug() << "File modified: " << path;

    QFileInfo info(path);
    QString hash(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex());
    emit this->fileAddedSignal(info, hash, info.lastModified().toTime_t());
}
