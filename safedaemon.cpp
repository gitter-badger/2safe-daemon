#include "safedaemon.h"

SafeDaemon::SafeDaemon(QObject *parent) : QObject(parent) {
    this->settings = new QSettings(ORG_NAME, APP_NAME, this);
    this->apiFactory = new SafeApiFactory(API_HOST, this);
    this->server = new QLocalServer(this);

    connect(server, &QLocalServer::newConnection, this, &SafeDaemon::handleClientConnection);
    this->bindServer(this->server,
                     QDir::homePath() +
                     QDir::separator() + SAFE_DIR +
                     QDir::separator() + SOCKET_FILE);

    if (this->authUser()) {
        this->initDatabase(STATE_DATABASE);
        this->initWatcher(getFilesystemPath());
    }
}

SafeDaemon::~SafeDaemon()
{
    this->apiFactory->deleteLater();
    this->watcher->deleteLater();
    this->database.close();
}

bool SafeDaemon::authUser() {
    /* Credentials */
    QString login = this->settings->value("login", "").toString();
    QString password = this->settings->value("password", "").toString();

    if (login.length() < 1 || password.length() < 1) {
        qDebug() << "Unauthorized";
        return false;
    }

    /* Authentication & FS initialization */
    if(!this->apiFactory->authUser(login, password)) {
        qWarning() << "Authentication failed";

        return false;
    }

    return true;
}

void SafeDaemon::deauthUser()
{
    this->apiFactory->deleteLater();
    this->watcher->deleteLater();
    this->database.close();
    this->settings->setValue("login", "");
    this->settings->setValue("password", "");
    this->apiFactory = new SafeApiFactory(API_HOST, this);
}


void SafeDaemon::initWatcher(const QString &path) {
    this->watcher = new FSWatcher(path, this);

    connect(this->watcher, &FSWatcher::added, this, &SafeDaemon::fileAdded);
    connect(this->watcher, &FSWatcher::modified, this, &SafeDaemon::fileModified);

    /*
    QDirIterator iterator(path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    this->indexDirectory(path);
    while (iterator.hasNext()) {
        this->indexDirectory(iterator.next());
    }
    */

    this->watcher->watch();
}

void SafeDaemon::initDatabase(const QString &name) {
    QString databaseDirectory = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    if (databaseDirectory.isEmpty()) {
        qDebug() << "Can not find database location";
    } else {
        if (!QDir(databaseDirectory).exists()) {
            QDir().mkpath(databaseDirectory);
        }

        QString databasePath = QDir(databaseDirectory).filePath(name);
        qDebug() << "Using database path:" << databasePath;

        this->database = QSqlDatabase::addDatabase("QSQLITE");
        this->database.setDatabaseName(databasePath);

        if (this->database.open()) {
            this->createDatabase();
        } else {
            qDebug() << "Can not open database";
        }
    }
}

void SafeDaemon::createDatabase() {
    QSqlQuery query(this->database);
    query.prepare("CREATE TABLE IF NOT EXISTS files (hash VARCHAR(32) PRIMARY KEY, path TEXT, updated_at INTEGER)");

    if (!query.exec()) {
        qDebug() << "Can not run database query";
    }
}

bool SafeDaemon::isListening() {
    return this->server->isListening();
}

QString SafeDaemon::socketPath() {
    return this->server->fullServerName();
}

void SafeDaemon::finishTransfer(const QString &path)
{
    activeTransfers.take(path)->deleteLater();
}

QString SafeDaemon::getFilesystemPath()
{
    QString root = this->settings->value("root_name", DEFAULT_ROOT_NAME).toString();
    return QDir::cleanPath(QDir::homePath() + QDir::separator() + root);
}

QJsonObject SafeDaemon::formSettingsReply(const QJsonArray &requestFields) {
    QJsonObject result, values;

    result.insert("type", QJsonValue(QString("settings")));
    foreach (auto field, requestFields) {
        QString value = this->settings->value(field.toString(), "").toString();
        if (value.length() > 0) {
            values.insert(field.toString(), value);
        }
    }
    result.insert("values", values);
    return result;
}

void SafeDaemon::bindServer(QLocalServer *server, QString path)
{
    QFile socket_file(path);
    if (!server->listen(path)) {
        /* try to remove old socket file */
        if (socket_file.exists() && socket_file.remove()) {
            /* retry bind */
            if (!server->listen(path)) {
                qWarning() << "Unable to bind socket to" << path;
            }
        } else {
            qWarning() << "Unable to bind socket on" << path << ", try to remove it manually.";
        }
    }
}

void SafeDaemon::handleClientConnection()
{
    auto socket = this->server->nextPendingConnection();
    while (socket->bytesAvailable() < 1) {
        socket->waitForReadyRead();
    }

    QObject::connect(socket, &QLocalSocket::disconnected, &QLocalSocket::deleteLater);
    if (!socket->isValid() || socket->bytesAvailable() < 1) {
        return;
    }

    qDebug() << "Handling incoming connection";
    QTextStream stream(socket);
    stream.autoDetectUnicode();
    QString data(stream.readAll());
    qDebug() << "Data read:" << data;

    /* JSON parsing */
    QJsonParseError jsonError;
    QJsonDocument jsonMessage = QJsonDocument::fromJson(data.toLatin1(), &jsonError);
    if (jsonError.error) {
        qWarning() << "JSON error:" << jsonError.errorString();
        return;
    } else if (!jsonMessage.isObject()) {
        qWarning() << "Not an object:" << jsonMessage;
        return;
    }

    /* Login */
    QJsonObject message = jsonMessage.object();
    QString type = message["type"].toString();

    if (type == GET_SETTINGS_TYPE) {
        QJsonObject response = formSettingsReply(message["fields"].toArray());
        stream <<  QJsonDocument(response).toJson();
        stream.flush();
    } else if (type == SET_SETTINGS_TYPE) {
        QJsonObject args = message["args"].toObject();
        for (QJsonObject::ConstIterator i = args.begin(); i != args.end(); ++i) {
            this->settings->setValue(i.key(), i.value().toString());
        }
    } else if (type == API_CALL) {
        // XXX
    } else if (type == NOOP) {
        stream << "noop.";
        stream.flush();
    } else {
        qWarning() << "Got message of unknown type:" << type;
    }

    socket->close();
}

void SafeDaemon::indexDirectory(const QString &path) {
    qDebug() << "Indexing" << path;

    QDirIterator iterator(path, QDir::Files);

    while (iterator.hasNext()) {
        iterator.next();

        QFileInfo info = iterator.fileInfo();
        QString hash(makeHash(iterator.filePath()));
        QDateTime updatedAtFs = info.lastModified();

        QSqlQuery query(this->database);
        // Why the fuck bindValue does not work?
        query.prepare("SELECT * FROM files WHERE hash = :hash");
        query.bindValue(":hash", hash);

        if (!query.exec()) {
            qDebug() << "Can not run database query" << query.lastError().text();
        } else {
            QSqlRecord record = query.record();

            if (query.next()) {
                uint updatedAtDb = query.value(record.indexOf("updated_at")).toUInt();

                if (updatedAtFs.toTime_t() != updatedAtDb) {
                    qDebug() << "[x] File modified:" << iterator.filePath();
                    emit this->fileModified(iterator.filePath());
                }
            } else {
                qDebug() << "[x] File added:" << iterator.filePath();
                emit this->fileAdded(iterator.filePath(), false); // XXX
            }
        }
    }
}

void SafeDaemon::fileAdded(const QString &path, bool isDir) {
    qDebug() << "File added: " << path;

    QFileInfo info(path);
    QString hash(makeHash(path));
    uint updatedAt = info.lastModified().toTime_t();
    QString fileName = info.fileName();

    if (!this->isFileAllowed(info)) {
        qDebug() << "Ignoring file" << path;

        return;
    }

    qDebug() << "Uploading new file" << path;

    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::pushFileProgress, [=](ulong id, ulong bytes, ulong totalBytes){
        qDebug() << "Progress:" << bytes << "/" << totalBytes;
    });
    connect(api, &SafeApi::pushFileComplete, [=](ulong id, SafeFile fileInfo) {
        qDebug() << "New file uploaded:" << fileInfo.name;

        insertFileInfo(path, hash, updatedAt);
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error:" << text << "(" << code << ")";
        finishTransfer(path);
    });

    bool active = this->activeTransfers.contains(path);
    if(active) {
        finishTransfer(path);
    }
    this->activeTransfers.insert(path, api);
    api->pushFile("227930033757", path, fileName, active);
}

void SafeDaemon::fileModified(const QString &path) {
    qDebug() << "File modified: " << path;

    QFileInfo info(path);
    QString hash(makeHash(path));
    uint updatedAt = info.lastModified().toTime_t();
    QString fileName = info.fileName();

    if (!this->isFileAllowed(info)) {
        qDebug() << "Ignoring file" << path;
        return;
    }

    qDebug() << "Uploading file" << path;

    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::pushFileProgress, [=](ulong id, ulong bytes, ulong totalBytes){
        qDebug() << "Progress:" << bytes << "/" << totalBytes;
    });
    connect(api, &SafeApi::pushFileComplete, [=](ulong id, SafeFile fileInfo){
        qDebug() << "File uploaded:" << fileInfo.name;

        updateFileInfo(path, hash, updatedAt);
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error:" << text << "(" << code << ")";
        finishTransfer(path);
    });

    bool active = this->activeTransfers.contains(path);
    if(active) {
        finishTransfer(path);
    }
    this->activeTransfers.insert(path, api);
    api->pushFile("227930033757", path, fileName, active);
}

void SafeDaemon::insertFileInfo(const QString &path, const QString &hash, const uint &updatedAt) {
    QSqlQuery query(this->database);
    query.prepare("INSERT INTO files (hash, path, updated_at) VALUES (:hash, :path, :updated_at)");
    query.bindValue(":hash", hash);
    query.bindValue(":path", path);
    query.bindValue(":updated_at", updatedAt);

    if (!query.exec()) {
        qDebug() << "Can not run database query:" << query.lastError().text();
    }
}

void SafeDaemon::updateFileInfo(const QString &path, const QString &hash, const uint &updatedAt) {
    QSqlQuery query(this->database);
    query.prepare("UPDATE files SET path = :path, updated_at = :updated_at WHERE hash = :hash");
    query.bindValue(":hash", hash);
    query.bindValue(":path", path);
    query.bindValue(":updated_at", updatedAt);

    if (!query.exec()) {
        qDebug() << "Can not run database query:" << query.lastError().text();
    }
}

bool SafeDaemon::isFileAllowed(const QFileInfo &info) {
    return !info.isHidden();
}

QString SafeDaemon::makeHash(const QString &path)
{
    QString hash(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex());
    return hash;
}
