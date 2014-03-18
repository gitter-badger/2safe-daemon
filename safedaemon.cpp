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

        fullRemoteIndex();

        if(this->settings->value("init", true).toBool()) {
            purgeDb(LOCAL_STATE_DATABASE);
            fullIndex(QDir(getFilesystemPath()));
            //this->settings->setValue("init", false);
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
    connect(this->watcher, &FSWatcher::deleted, this, &SafeDaemon::fileDeleted);
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
    if(activeTransfers.contains(path))
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
    if (!this->isFileAllowed(info)) {
        qDebug() << "Ignoring object" << info.filePath();
        return;
    }

    if(isDir) {
        info.setFile(QDir(path).path());
        qDebug() << "Directory created: " << info.filePath();
    } else {
        info.setFile(path);
        qDebug() << "File added: " << info.filePath();
    }

    if(isDir) {
        QString dirId = createDir(getDirId(relativePath(info)), info.filePath());
        //this->localStateDb->updateDirId(relativePath(info), dirId);
        this->localStateDb->removeDir(relativeFilePath(info));
        this->localStateDb->insertDir(relativeFilePath(info),
                                      info.dir().dirName(), getMtime(info), dirId);
        // fullIndex
        return;
    }

    queueUploadFile(getDirId(relativePath(info)), info);
    this->localStateDb->insertFile(relativePath(info), relativeFilePath(info),
                                   info.fileName(), getMtime(info), makeHash(info));
    this->localStateDb->updateDirHash(relativePath(info));
}

void SafeDaemon::fileModified(const QString &path) {
    QFileInfo info(path);
    if (!this->isFileAllowed(info)) {
        qDebug() << "Ignoring object" << info.filePath();
        return;
    } else {
        qDebug() << "File modified: " << info.filePath();
    }

    queueUploadFile(getDirId(relativePath(info)), info);
    this->localStateDb->removeFile(relativeFilePath(info));
    this->localStateDb->insertFile(relativePath(info), relativeFilePath(info),
                                   info.fileName(), getMtime(info), makeHash(info));
    this->localStateDb->updateDirHash(relativePath(info));
}

void SafeDaemon::fileDeleted(const QString &path, bool isDir)
{
    QFileInfo info(path);
    if (!this->isFileAllowed(info)) {
        qDebug() << "Ignoring object" << info.filePath();
        return;
    }

    if(isDir) {
        qDebug() << "Directory deleted: " << info.filePath();
    } else {
        qDebug() << "File deleted: " << info.filePath();
    }

    if(isDir) {
        removeDir(relativePath(info));
        this->localStateDb->removeDir(relativeFilePath(info));
        return;
    }
    return;
    /*
    uploadFile(getDirId(relativePath(info)), info);
    this->localStateDb->insertFile(relativePath(info), relativeFilePath(info),
                                   info.fileName(), makeHash(info), getMtime(info));
    updateDirHash(info.dir());
    */
}

void SafeDaemon::fileMoved(const QString &path1, const QString &path2)
{

}

void SafeDaemon::fileCopied(const QString &path1, const QString &path2)
{

}

QString SafeDaemon::createDir(const QString &parent_id, const QString &path)
{
    QEventLoop loop;
    QString dirId;
    qDebug() << "Creating new directory" << path;

    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::makeDirComplete, [&](ulong id, ulong dir_id){
        qDebug() << "Created directory" << dir_id << "in" << parent_id;
        dirId = dir_id;
        loop.exit();
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error making dir:" << text << "(" << code << ")";
        loop.exit();
    });

    api->makeDir(parent_id, QDir(path).dirName());
    loop.exec();
    return dirId;
}

void SafeDaemon::removeDir(const QString &path)
{
    QEventLoop loop;
    qDebug() << "Removing directory" << path;

    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::removeDirComplete, [&](ulong id){
        qDebug() << "Removed successfully" << path;
        loop.exit();
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error removing dir:" << text << "(" << code << ")";
        loop.exit();
    });

    api->removeDir(path, true, true);
    loop.exec();
}

void SafeDaemon::queueUploadFile(const QString &dir_id, const QFileInfo &info)
{
    QTimer *timer = new QTimer(this);
    QString path(info.filePath());

    timer->setInterval(2000);
    timer->setSingleShot(true);
    timer->setTimerType(Qt::VeryCoarseTimer);

    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::pushFileProgress, [&](ulong id, ulong bytes, ulong totalBytes){
        qDebug() << "Progress:" << bytes << "/" << totalBytes;
    });
    connect(api, &SafeApi::pushFileComplete, [&](ulong id, SafeFile fileInfo) {
        qDebug() << "New file uploaded:" << fileInfo.name;
        finishTransfer(path);
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

    connect(timer, &QTimer::timeout, [=](){
        uploadFile(dir_id, info);
    });

    bool queued = this->pendingTransfers.contains(path);
    if(queued) {
        this->pendingTransfers[path]->stop();
        this->pendingTransfers.take(path)->deleteLater();
    }
    this->pendingTransfers.insert(path, timer);
    timer->start();
}

void SafeDaemon::uploadFile(const QString &dir_id, const QFileInfo &info)
{
    QString path(info.filePath());
    this->activeTransfers[path]->pushFile(dir_id, path, info.fileName(), true);
}

void SafeDaemon::removeFile(const QFileInfo &info)
{
    QString path(info.filePath());
    QEventLoop loop;

    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::removeFileComplete, [&](ulong id){
        qDebug() << "File deleted" << path;
        loop.exit();
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error:" << text << "(" << code << ")";
        loop.exit();
    });

    bool queued = this->pendingTransfers.contains(path);
    if(queued) {
        this->pendingTransfers[path]->stop();
        this->pendingTransfers.take(path)->deleteLater();
    }

    finishTransfer(path);
    api->removeFile(QString(/* id here */), true);
    loop.exec();
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

ulong SafeDaemon::getMtime(const QFileInfo &info)
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
    QMap<QString, QPair<QString, ulong> > dir_index;
    QDirIterator iterator(dir.absolutePath(), QDirIterator::Subdirectories);
    struct s {
        ulong space = 0;
        ulong files = 0;
        ulong dirs = 0;
    } stats;

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
            //index file
            stats.files++;
            this->localStateDb->insertFile(
                        relativePath(info),
                        relativeFilePath(info),
                        info.fileName(),
                        mtime, hash);

            if(!dir_index.contains(dir)){
                // index dir
                dir_index.insert(dir, QPair<QString, ulong>(
                                     hash, mtime));
                continue;
            }
            dir_index[dir].first.append(hash);
            if(mtime > dir_index[dir].second) {
                dir_index[dir].second = mtime;
            }
        } else if (QDir(info.filePath()).count() < 3) {
            // index empty dir
            this->localStateDb->insertDir(relativeFilePath(info),
                                          info.dir().dirName(),
                                          getMtime(info));
        }
    }

    foreach(auto k, dir_index.keys()) {
        QString relative = relativeFilePath(k);
        if(relative.isEmpty()) {
            continue;
        }
        stats.dirs++;
        localStateDb->insertDir(relative, QDir(k).dirName(),
                                dir_index[k].second, makeHash(dir_index[k].first));
    }
    qDebug() << "GBs:" << stats.space / (1024.0 * 1024.0 * 1024.0)
             << "\nFiles:" << stats.files <<
                "\nDirs:" << stats.dirs;
}

void SafeDaemon::fullRemoteIndex()
{
    QEventLoop loop;
    auto api = this->apiFactory->newApi();
    uint counter = 0;

    connect(api, &SafeApi::listDirComplete, [&](ulong id, QList<SafeDir> dirs,
            QList<SafeFile> files, QJsonObject root_info){
        bool root = false;
        QString tree = root_info.value("tree").toString();
        tree.remove(0, 1);
        tree.chop(1);
        if(tree.isEmpty()){
            root = true;
            tree = QString(QDir::separator());
        }

        foreach(SafeFile file, files) {
            if(file.is_trash) {
                continue;
            }
            remoteStateDb->insertFile(tree, root ? file.name : (tree + QDir::separator() + file.name),
                                      file.name, file.mtime, file.chksum, file.id);
        }

        foreach(SafeDir dir, dirs) {
            if(dir.is_trash || !dir.special_dir.isEmpty()) {
                continue;
            }
            ++counter;
            remoteStateDb->insertDir(root ? dir.name : (tree + QDir::separator() + dir.name),
                                     dir.name, dir.mtime, dir.id);
            api->listDir(dir.id);
        }

        --counter; // dir parsed
        if(dirs.isEmpty() && counter < 1) {
            // no more dirs for recursion
            loop.exit();
            return;
        }
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error:" << text << "(" << code << ")";
        --counter;
        if(counter < 1)
            loop.exit();

    });

    ++counter;
    api->listDir(getDirId("/"));
    loop.exec();

    qDebug() << "Finished indexing" << counter;
}

void SafeDaemon::checkIndex(const QDir &dir)
{

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
