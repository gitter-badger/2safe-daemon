#include "safefilesystem.h"

SafeFileSystem::SafeFileSystem(const QString &path, const QString &databaseName, bool debug = false, QObject *parent = 0) : QObject(parent) {
    this->debug = debug;
    this->directory = path;
    this->initDatabase(databaseName);
    this->createDatabase();
    this->initWatcher(path);
}

SafeFileSystem::~SafeFileSystem() {

}

/*
 * Establish a connection to SQLite database
 */
void SafeFileSystem::initDatabase(const QString &databaseName) {
    QString databaseDirectory = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    if (databaseDirectory.isEmpty()) {
        this->log("Can not find database location");
    } else {
        if (!QDir(databaseDirectory).exists()) {
            QDir().mkdir(databaseDirectory);
        }

        QString databasePath = QDir(databaseDirectory).filePath(databaseName);
        this->log("Using database path: " + databasePath);

        this->database = QSqlDatabase::addDatabase("QSQLITE");
        this->database.setDatabaseName(databasePath);

        if (!this->database.open()) {
            this->log("Can not open database");
        }
    }
}

/*
 * Recursively scan 2Safe root directory and add all subdirectories to watchlist
 */
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

/*
 * Recursively iterate a given directory and find the files that have been modified
 */
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
            this->log("Can not run database query");
        } else {
            QSqlRecord record = selectQuery.record();

            if (selectQuery.next()) {
                uint updatedAtDb = selectQuery.value(record.indexOf("updated_at")).toUInt();

                if (updatedAtFs.toTime_t() != updatedAtDb) {
                    this->updateFileInfo(iterator.filePath(), hash, updatedAtFs.toTime_t());
                    this->log("File modified: (" + QString::number(updatedAtDb) + ", " + QString::number(updatedAtFs.toTime_t()) + ")" + iterator.filePath());
                }
            } else {
                this->saveFileInfo(iterator.filePath(), hash, updatedAtFs.toTime_t());
                this->log("File added: " + iterator.filePath());
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
        this->log("Can not run database query: " + query.lastError().text());
    }
}

void SafeFileSystem::updateFileInfo(const QString &path, const QString &hash, const uint &updatedAt) {
    QSqlQuery query(this->database);
    query.prepare("UPDATE files SET path = :path, updated_at = :updated_at WHERE hash = :hash");
    query.bindValue(":hash", hash);
    query.bindValue(":path", path);
    query.bindValue(":updated_at", updatedAt);

    if (!query.exec()) {
        this->log("Can not run database query: " + query.lastError().text());
    }
}

void SafeFileSystem::createDatabase() {
    QSqlQuery query(this->database);
    query.prepare("CREATE TABLE IF NOT EXISTS files (hash VARCHAR(32) PRIMARY KEY, path TEXT, updated_at INTEGER)");

    if (!query.exec()) {
        this->log("Can not run database query");
    }
}

/*
 * Slot for QFileSystemWatcher signal
 */
void SafeFileSystem::directoryChanged(const QString &path) {
    this->reindexDirectory(path);
}

/*
 * Dummy log method
 */
void SafeFileSystem::log(const QString &str) {
    if (this->debug) {
        QFile logFile("/Users/awolf/2safe.log");

        if (logFile.open(QIODevice::Append)) {
            QTextStream logStream(&logFile);
            logStream << str << "\n";
            logFile.close();
        }
    }
}
