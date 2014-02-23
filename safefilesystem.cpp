#include "safefilesystem.h"

SafeFileSystem::SafeFileSystem(const QString &path, const QString &databaseName, QObject *parent = 0) : QObject(parent) {
    this->directory = path;
    this->databaseName = databaseName;
}

SafeFileSystem::~SafeFileSystem() {
}

void SafeFileSystem::startWatching() {
    this->initDatabase(this->databaseName);
    this->createDatabase();
    this->initWatcher(this->directory);
}

void SafeFileSystem::initDatabase(const QString &databaseName) {
    QString databaseDirectory = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    if (databaseDirectory.isEmpty()) {
        qDebug() << "Can not find database location";
    } else {
        if (!QDir(databaseDirectory).exists()) {
            QDir().mkdir(databaseDirectory);
        }

        QString databasePath = QDir(databaseDirectory).filePath(databaseName);
        qDebug() << "Using database path:" << databasePath;

        this->database = QSqlDatabase::addDatabase("QSQLITE");
        this->database.setDatabaseName(databasePath);

        if (!this->database.open()) {
            qDebug() << "Can not open database";
        }
    }
}

void SafeFileSystem::initWatcher(const QString &path) {
    QDirIterator directoryIterator(path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QStringList directories;

    directories.append(this->directory);
    while (directoryIterator.hasNext()) {
        directories.append(directoryIterator.next());
    }

    this->watcher.addPaths(directories);
    connect(&this->watcher, &QFileSystemWatcher::directoryChanged, this, &SafeFileSystem::directoryChanged);

    QStringListIterator stringIterator(directories);
    while (stringIterator.hasNext()) {
         this->reindexDirectory(stringIterator.next());
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
                    //this->updateFileInfo(iterator.filePath(), hash, updatedAtFs.toTime_t());
                    qDebug() << "File modified: (" << QString::number(updatedAtDb) << "," << QString::number(updatedAtFs.toTime_t()) << ")" << iterator.filePath();
                    emit this->fileChanged(iterator.fileInfo(), hash, updatedAtFs.toTime_t());
                }
            } else {
                //this->saveFileInfo(iterator.filePath(), hash, updatedAtFs.toTime_t());
                qDebug() << "File added:" << iterator.filePath();
                emit this->fileAdded(iterator.fileInfo(), hash, updatedAtFs.toTime_t());
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

void SafeFileSystem::directoryChanged(const QString &path) {
    this->reindexDirectory(path);
}
