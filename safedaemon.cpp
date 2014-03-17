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
        this->localStateDb = new SafeStateDb(LOCAL_STATE_DATABASE);
        this->remoteStateDb = new SafeStateDb(REMOTE_STATE_DATABASE);
        if(this->settings->value("init", true).toBool()) {
            fullIndex(QDir(getFilesystemPath()));
            //this->settings->setValue("init", false);
            purgeDb(LOCAL_STATE_DATABASE);
        } else {
            checkIndex(QDir(getFilesystemPath()));
        }
        this->initWatcher(getFilesystemPath());
    }
}

SafeDaemon::~SafeDaemon()
{
    this->apiFactory->deleteLater();
    this->watcher->deleteLater();
    this->localStateDb->close();
    this->remoteStateDb->close();
    this->localStateDb->deleteLater();
    this->remoteStateDb->deleteLater();
}

bool SafeDaemon::authUser() {
    /* Credentials */
    QString login = this->settings->value("login", "").toString();
    QString password = this->settings->value("password", "").toString();

    if (login.length() < 1 || password.length() < 1) {
        qDebug() << "Unauthorized";
        return false;
    } else if(!this->apiFactory->authUser(login, password)) {
        qWarning() << "Authentication failed";
        return false;
    }

    return true;
}

void SafeDaemon::deauthUser()
{
    this->apiFactory->deleteLater();
    this->watcher->deleteLater();
    this->localStateDb->close();
    this->remoteStateDb->close();
    this->localStateDb->deleteLater();
    this->remoteStateDb->deleteLater();
    this->settings->setValue("login", "");
    this->settings->setValue("password", "");
    this->settings->setValue("init", true);

    this->apiFactory = new SafeApiFactory(API_HOST, this);
    purgeDb(LOCAL_STATE_DATABASE);
    purgeDb(REMOTE_STATE_DATABASE);
}

void SafeDaemon::purgeDb(const QString &name)
{
    QString path = SafeStateDb::formPath(name);
    QFile(path).remove();
}

void SafeDaemon::initWatcher(const QString &path) {
    this->watcher = new FSWatcher(path, this);
    connect(this->watcher, &FSWatcher::added, this, &SafeDaemon::fileAdded);
    connect(this->watcher, &FSWatcher::modified, this, &SafeDaemon::fileModified);
    connect(this->watcher, &FSWatcher::deleted, [](QString path, bool is_dir){
        qDebug() << "file deleted:" << path;
    });
    connect(this->watcher, &FSWatcher::moved, [](QString path1, QString path2, bool is_dir){
        qDebug() << "Moved" << path1 << "to" << path2;
    });
    this->watcher->watch();
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

void SafeDaemon::fileAdded(const QString &path, bool isDir) {
    QFileInfo info;
    if(isDir) {
        info.setFile(QDir(path).path());
        qDebug() << "Directory created: " << info.path();
    } else {
        info.setFile(path);
        qDebug() << "File added: " << info.path();
    }

    if (!this->isFileAllowed(info)) {
        qDebug() << "Ignoring file" << path;
        return;
    }

    uint mtime = info.lastModified().toTime_t();

    //QString hash(makeHash(path));

    if(isDir) {
        QEventLoop loop;
        QString dirId(getDirId(relativePath(info)));
        qDebug() << "Uploading new directory" << path;

        auto api = this->apiFactory->newApi();
        connect(api, &SafeApi::makeDirComplete, [&](ulong id, ulong dir_id){
            qDebug() << "Created directory" << dir_id << "in"
                     << dirId << "(" << (relativePath(info)) << ")";
            loop.exit();
        });
        connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
            qWarning() << "Error making dir:" << text << "(" << code << ")";
            loop.exit();
        });

        api->makeDir(dirId, info.fileName());
        loop.exec();
    }

    /*
    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::pushFileProgress, [=](ulong id, ulong bytes, ulong totalBytes){
        qDebug() << "Progress:" << bytes << "/" << totalBytes;
    });
    connect(api, &SafeApi::pushFileComplete, [=](ulong id, SafeFile fileInfo) {
        qDebug() << "New file uploaded:" << fileInfo.name;

        //insertFileInfo(path, hash, updatedAt);
    });
    connect(api, &SafeApi::errorRaised, [](ulong id, quint16 code, QString text){
        qWarning() << "Error:" << text << "(" << code << ")";
        finishTransfer(path);
    });

    bool active = this->activeTransfers.contains(path);
    if(active) {
        finishTransfer(path);
    }
    this->activeTransfers.insert(path, api);
    api->pushFile("227930033757", path, fileName, active);
    */
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

    /*
    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::pushFileProgress, [=](ulong id, ulong bytes, ulong totalBytes){
        qDebug() << "Progress:" << bytes << "/" << totalBytes;
    });
    connect(api, &SafeApi::pushFileComplete, [=](ulong id, SafeFile fileInfo){
        qDebug() << "File uploaded:" << fileInfo.name;

        //updateFileInfo(path, hash, updatedAt);
    });
    connect(api, &SafeApi::errorRaised, [](ulong id, quint16 code, QString text){
        qWarning() << "Error:" << text << "(" << code << ")";
        finishTransfer(path);
    });

    bool active = this->activeTransfers.contains(path);
    if(active) {
        finishTransfer(path);
    }
    this->activeTransfers.insert(path, api);
    api->pushFile("227930033757", path, fileName, active);
    */
}

void SafeDaemon::fileDeleted(const QString &path)
{

}

void SafeDaemon::fileMoved(const QString &path1, const QString &path2)
{

}

bool SafeDaemon::isFileAllowed(const QFileInfo &info) {
    return !info.isHidden();
}

QString SafeDaemon::makeHash(const QFileInfo &info)
{
    QFile file(info.filePath());
    if(!file.open(QFile::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result().toHex();
}

QString SafeDaemon::makeHash(const QString &str)
{
    QString hash(QCryptographicHash::hash(
                     str.toUtf8(), QCryptographicHash::Md5).toHex());
    return hash;
}

QString SafeDaemon::updateDirHash(const QDir &dir)
{
    qDebug() << "PATH:::" << dir.absolutePath();
    this->localStateDb->updateDirHash(relativeFilePath(
                                          QFileInfo(dir.absolutePath())));
}

uint SafeDaemon::getMtime(const QFileInfo &info)
{
    return info.lastModified().toTime_t();
}

// Seems that I'm supposed to know some O-effective algorithms,
// trees, rbtrees, other things,
// but I only know loops and ifs
// LOL
void SafeDaemon::fullIndex(const QDir &dir)
{
    qDebug() << "Doing full index";
    QMap<QString, QPair<QString, uint> > dir_index;
    QDirIterator iterator(dir.absolutePath(), QDirIterator::Subdirectories);
    struct s {
        uint space = 0;
        uint files = 0;
        uint dirs = 0;
    } stats;

    if (!this->localStateDb->open())
        return;
    while (iterator.hasNext()) {
        iterator.next();
        if(iterator.fileName() == "." || iterator.fileName() == "..") {
            continue;
        }
        auto info = iterator.fileInfo();
        if(info.isSymLink()) {
            continue;
        }
        if (!info.isDir()) {
            stats.space += info.size();
            auto hash = makeHash(info);
            auto mtime = getMtime(info);
            auto dir = info.absolutePath();
            this->localStateDb->insertFile(
                        relativePath(info),
                        relativeFilePath(info),
                        info.fileName(),
                        hash, mtime);

            if(!dir_index.contains(dir)){
                // index file
                stats.files++;
                dir_index.insert(dir, QPair<QString, uint>(
                                     hash, mtime));
                continue;
            }
            dir_index[dir].first.append(hash);
            if(mtime > dir_index[dir].second) {
                dir_index[dir].second = mtime;
            }
        } else if (QDir(info.filePath()).count() < 3) {
            // index empty dir
            stats.dirs++;
            localStateDb->insertDir(QString(), relativeFilePath(info),
                                    info.dir().dirName(), QString(),
                                    getMtime(info));
        }
    }

    foreach(auto k, dir_index.keys()) {
        QString relative = relativeFilePath(k);
        if(relative.isEmpty()) {
            continue;
        }
        // index dir
        stats.dirs++;
        localStateDb->insertDir(QString(), relative, QDir(k).dirName(),
                                makeHash(dir_index[k].first), dir_index[k].second);
    }
    this->localStateDb->close();
    qDebug() << "GBs:" << stats.space / (1024.0 * 1024.0 * 1024.0)
             << "\nFiles:" << stats.files <<
                "\nDirs:" << stats.dirs;
}

void SafeDaemon::checkIndex(const QDir &dir)
{
    /*
    qDebug() << "Doing full index";
    QMap<QString, QPair<QString, uint> > dir_index;
    QDirIterator iterator(dir.absolutePath(), QDirIterator::Subdirectories);
    struct s {
        uint space = 0;
        uint files = 0;
        uint dirs = 0;
    } stats;

    if (!this->localStateDb->open())
        return;
    while (iterator.hasNext()) {
        iterator.next();
        if(iterator.fileName() == "." || iterator.fileName() == "..") {
            continue;
        }
        auto info = iterator.fileInfo();
        if(info.isSymLink()) {
            continue;
        }
        if (!info.isDir()) {
            stats.space += info.size();
            auto hash = makeHash(info);
            auto mtime = getMtime(info);
            auto dir = info.absolutePath();
            this->localStateDb->insertFile(
                        relativePath(info),
                        relativeFilePath(info),
                        info.fileName(),
                        hash, mtime);

            if(!dir_index.contains(dir)){
                // index file
                stats.files++;
                dir_index.insert(dir, QPair<QString, uint>(
                                     hash, mtime));
                continue;
            }
            dir_index[dir].first.append(hash);
            if(mtime > dir_index[dir].second) {
                dir_index[dir].second = mtime;
            }
        } else if (QDir(info.filePath()).count() < 3) {
            // index empty dir
            stats.dirs++;
            localStateDb->insertDir(QString(), relativeFilePath(info),
                                    info.dir().dirName(), QString(),
                                    getMtime(info));
        }
    }

    foreach(auto k, dir_index.keys()) {
        QString relative = relativeFilePath(k);
        if(relative.isEmpty()) {
            continue;
        }
        // index dir
        stats.dirs++;
        localStateDb->insertDir(QString(), relative, QDir(k).dirName(),
                                makeHash(dir_index[k].first), dir_index[k].second);
    }
    this->localStateDb->close();
    qDebug() << "GBs:" << stats.space / (1024.0 * 1024.0 * 1024.0)
             << "\nFiles:" << stats.files <<
                "\nDirs:" << stats.dirs;
                */
}

QString SafeDaemon::relativeFilePath(const QFileInfo &info)
{
    QString relative = QDir(getFilesystemPath()).relativeFilePath(info.filePath());
    return relative.isEmpty() ? "/" : relative;
}

QString SafeDaemon::getDirId(const QString &path)
{
    QString dirId;
    QEventLoop loop;
    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::getPropsComplete, [&](ulong id, QJsonObject props){
        auto info = props.value("object").toObject();
        dirId = SafeDir(info).id;
        loop.exit();
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error getting props:" << text << "(" << code << ")";
        loop.exit();
    });

    api->getProps(path, true);
    loop.exec();
    return dirId;
}

QString SafeDaemon::relativePath(const QFileInfo &info)
{
    QString relative;
    if(info.isDir()) {
        relative = QDir(getFilesystemPath()).relativeFilePath(info.dir().path());
    } else {
        relative = QDir(getFilesystemPath()).relativeFilePath(info.path());
    }
    return relative.isEmpty() ? "/" : relative;
}
