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
        // debug clean dbs
        purgeDb(LOCAL_STATE_DATABASE);
        purgeDb(REMOTE_STATE_DATABASE);
        // open dbs
        this->localStateDb = new SafeStateDb(LOCAL_STATE_DATABASE);
        this->remoteStateDb = new SafeStateDb(REMOTE_STATE_DATABASE);
        // index all remote files
        fullRemoteIndex();
        // setup watcher (to track remote events from now)
        this->settings->setValue("last_updated", (quint32)QDateTime::currentDateTime().toTime_t());
        this->swatcher = new SafeWatcher((ulong)this->settings->value("last_updated").toDouble(),
                                         this->apiFactory, this);
        connect(this->swatcher, &SafeWatcher::timestampChanged, [&](ulong ts){
            this->settings->setValue("last_updated", (quint32)ts);
        });
        connect(this->swatcher, &SafeWatcher::fileAdded, this, &SafeDaemon::remoteFileAdded);
        connect(this->swatcher, &SafeWatcher::fileDeleted, this, &SafeDaemon::remoteFileDeleted);
        connect(this->swatcher, &SafeWatcher::fileMoved, this, &SafeDaemon::remoteFileMoved);
        connect(this->swatcher, &SafeWatcher::directoryCreated, this, &SafeDaemon::remoteDirectoryCreated);
        connect(this->swatcher, &SafeWatcher::directoryDeleted, this, &SafeDaemon::remoteDirectoryDeleted);
        connect(this->swatcher, &SafeWatcher::directoryMoved, this, &SafeDaemon::remoteDirectoryMoved);

        // local index
        if(this->settings->value("init", true).toBool()) {
            fullIndex(QDir(getFilesystemPath()));
            //this->settings->setValue("init", false);
        } else {
            checkIndex(QDir(getFilesystemPath()));
        }
        // start watching for remote events
        this->swatcher->watch();
        // start watching for fs events
        this->initWatcher(getFilesystemPath());
    }
}

SafeDaemon::~SafeDaemon()
{
    this->apiFactory->deleteLater();
    this->watcher->deleteLater();
    this->swatcher->deleteLater();
    this->localStateDb->deleteLater();
    this->remoteStateDb->deleteLater();
}

bool SafeDaemon::authUser() {
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
    connect(this->watcher, &FSWatcher::moved, this, &SafeDaemon::fileMoved);
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
        stream << "noop";
        stream.flush();
    } else {
        qWarning() << "Got message of unknown type:" << type;
    }

    socket->close();
}

QJsonObject SafeDaemon::fetchFileInfo(const QString &id)
{
    QJsonObject info;
    QEventLoop loop;
    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::getPropsComplete, [&](ulong id, QJsonObject props){
        info = props.value("object").toObject();
        loop.exit();
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error fetching info:" << text << "(" << code << ")";
        loop.exit();
    });

    api->getProps(id);
    loop.exec();
    return info;
}

QJsonObject SafeDaemon::fetchDirInfo(const QString &id)
{
    QJsonObject info;
    /* ABANDONED
    QEventLoop loop;
    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::getPropsComplete, [&](ulong id, QJsonObject props){
        info = props.value("object").toObject();
        loop.exit();
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error fetching info:" << text << "(" << code << ")";
        loop.exit();
    });

    api->getProps(id);
    loop.exec();
    */
    return info;
}

void SafeDaemon::fileAdded(const QString &path, bool isDir) {
    QFileInfo info;
    if (!this->isFileAllowed(info)) {
        qDebug() << "Ignoring object" << info.filePath();
        return;
    }

    if(isDir) {
        info.setFile(QDir(path).path());
    } else {
        info.setFile(path);
    }

    QString relative(relativePath(info));
    QString relativeF(relativeFilePath(info));

    if(isDir) {
        qDebug() << "Directory added: " << relativeF;

        if(this->remoteStateDb->existsDir(relativeF)) {
            return;
        }

        QString dirId = createDir(fetchDirId(relative), info.filePath());
        this->localStateDb->removeDir(relativeF);
        this->localStateDb->insertDir(relativeF, info.dir().dirName(), getMtime(info), dirId);
        // XXX: fullIndex(QDir(path));
        return;
    }

    this->localStateDb->removeFile(relativeF);
    this->localStateDb->insertFile(relative, relativeF, info.fileName(),
                                   getMtime(info), makeHash(info));
    this->localStateDb->updateDirHash(relative);

    if(this->remoteStateDb->existsFile(relativeF)){
        // XXX: check for cause (mtime/hash)
        return;
    }

    qDebug() << "File added: " << info.filePath();
    queueUploadFile(fetchDirId(relative), info);
}

void SafeDaemon::fileModified(const QString &path) {
    QFileInfo info(path);
    if (!this->isFileAllowed(info)) {
        qDebug() << "Ignoring object" << info.filePath();
        return;
    }

    QString relative(relativePath(info));
    QString relativeF(relativeFilePath(info));

    this->localStateDb->removeFile(relativeF);
    this->localStateDb->insertFile(relative, relativeF, info.fileName(),
                                   getMtime(info), makeHash(info));
    this->localStateDb->updateDirHash(relative);

    if(this->remoteStateDb->existsFile(relativeF)){
        // XXX: check for cause (mtime/hash)
        return;
    }

    qDebug() << "File modified: " << info.filePath();
    queueUploadFile(fetchDirId(relative), info);
}

void SafeDaemon::fileDeleted(const QString &path, bool isDir)
{
    QFileInfo info(path);
    QString relativeF(relativeFilePath(info));

    if (!this->isFileAllowed(info)) {
        qDebug() << "Ignoring object" << info.filePath();
        return;
    }

    if(isDir) {
        if(!this->localStateDb->existsDir(relativeF)) {
            //qDebug() << relativeF << "not tracked, ignore";
            return;
        }
        qDebug() << "Directory deleted: " << info.filePath();
    } else {
        if(!this->localStateDb->existsFile(relativeF)) {
            //qDebug() << relativeF << "not tracked, ignore";
            return;
        }
        qDebug() << "Local file deleted: " << info.filePath();
    }

    if(isDir) {
        this->localStateDb->removeDir(relativeF);
        this->localStateDb->removeDirRecursively(relativeF);
        remoteRemoveDir(info);
        return;
    }

    remoteRemoveFile(info);
    this->localStateDb->removeFile(relativeF);
    updateDirHash(info.dir());
}

void SafeDaemon::fileMoved(const QString &path1, const QString &path2)
{

}

void SafeDaemon::fileCopied(const QString &path1, const QString &path2)
{

}

void SafeDaemon::remoteFileAdded(QString id, QString pid, QString name)
{
    qDebug() << "[REMOTE EVENT] file added:" << name;
    QString dir = this->remoteStateDb->getDirPathById(pid);
    QString path = (dir == QString(QDir::separator()))
            ? name : (dir + QString(QDir::separator()) + name);
    SafeFile file(fetchFileInfo(id));

    this->remoteStateDb->removeFile(path);
    this->remoteStateDb->insertFile(dir, path, name, file.mtime, file.chksum, id);

    if(this->localStateDb->existsFile(path)) {
        return; // wait for cause (mtime, hash)
    }

    queueDownloadFile(id, getFilesystemPath() + QDir::separator() + path);
}

void SafeDaemon::remoteFileDeleted(QString id, QString pid, QString name)
{
    qDebug() << "[REMOTE EVENT] file deleted:" << name;
    QString dir = this->remoteStateDb->getDirPathById(pid);
    QString path = (dir == QString(QDir::separator()))
            ? name : (dir + QString(QDir::separator()) + name);
    this->remoteStateDb->removeFile(path);

    if(this->localStateDb->existsFile(path)) {
        this->localStateDb->removeFile(path);
        QFile(getFilesystemPath() + QDir::separator() + path).remove();
    }
}

void SafeDaemon::remoteDirectoryCreated(QString id, QString pid, QString name)
{
    qDebug() << "[REMOTE EVENT] directory created:" << name;
    QString dir = this->remoteStateDb->getDirPathById(pid);
    QString path = (dir == QString(QDir::separator()))
            ? name : (dir + QString(QDir::separator()) + name);
    SafeFile info(fetchDirInfo(id));

    this->remoteStateDb->removeDir(path);
    this->remoteStateDb->insertDir(path, name, info.mtime, id);

    if(this->localStateDb->existsDir(path)) {
        return;
    } else {
        this->localStateDb->insertDir(path, name, info.mtime, id);
    }

    QDir(getFilesystemPath()).mkdir(name);
}

void SafeDaemon::remoteDirectoryDeleted(QString id, QString pid, QString name)
{
    qDebug() << "[REMOTE EVENT] directory deleted:" << name << id;
    QString path(this->remoteStateDb->getDirPathById(id));
    qDebug() << "(path)" << path;
    this->remoteStateDb->removeDirById(id);
    this->remoteStateDb->removeDirByIdRecursively(id);

    if (this->localStateDb->existsDir(path)){
        qDebug() << "(exists, path)" << path;
        this->localStateDb->removeDir(path);
        this->localStateDb->removeDirRecursively(path);
    }

    if(path.length() > 1) {
        qDebug() << "(removing recusively)";
        QDir dir(getFilesystemPath() + QDir::separator() + path);
        if (dir.exists()) {
            dir.removeRecursively();
        }
    }
}

void SafeDaemon::remoteFileMoved(QString id, QString pid1, QString n1, QString pid2, QString n2)
{
    SafeDir dir1(fetchDirInfo(pid1));
    SafeDir dir2(fetchDirInfo(pid2));
    qDebug() << "[REMOTE EVENT] file moved:" << dir1.tree+n1 << "to" << dir2.tree+n2;
}

void SafeDaemon::remoteDirectoryMoved(QString id, QString pid1, QString n1, QString pid2, QString n2)
{
    SafeDir dir1(fetchDirInfo(pid1));
    SafeDir dir2(fetchDirInfo(pid2));
    qDebug() << "[REMOTE EVENT] directory moved:" << dir1.tree+n1 << "to" << dir2.tree+n2;
}

QString SafeDaemon::createDir(const QString &parent_id, const QString &path)
{
    QEventLoop loop;
    QString dirId;

    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::makeDirComplete, [&](ulong id, ulong dir_id){
        qDebug() << "Created remote directory:" << dir_id << "in" << parent_id;
        dirId = dir_id;
        loop.exit();
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error creating dir:" << text << "(" << code << ")";
        loop.exit();
    });

    api->makeDir(parent_id, QDir(path).dirName());
    loop.exec();
    return dirId;
}

void SafeDaemon::remoteRemoveDir(const QFileInfo &info)
{
    QEventLoop loop;
    QString relative(relativeFilePath(info));
    QString id(this->remoteStateDb->getDirId(relative));
    if(id.isEmpty()) {
        qWarning() << "Directory" << relative << "isn't exists in the remote index";
        return;
    }

    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::removeDirComplete, [&](ulong id){
        qDebug() << "Remote directory deleted:" << relative;
        loop.exit();
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error deleting remote dir:" << text << "(" << code << ")";
        loop.exit();
    });

    api->removeDir(id, true, true);
    loop.exec();
}

void SafeDaemon::queueUploadFile(const QString &dir_id, const QFileInfo &info)
{
    QTimer *timer = new QTimer(this);
    QString path(info.filePath());

    timer->setInterval(2000);
    timer->setSingleShot(true);
    timer->setTimerType(Qt::VeryCoarseTimer);

    bool active = this->activeTransfers.contains(path);
    if(active) {
        finishTransfer(path);
    }

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
    auto api = this->apiFactory->newApi();

    connect(api, &SafeApi::pushFileProgress, [=](ulong id, ulong bytes, ulong totalBytes){
        qDebug() << "U/Progress:" << bytes << "/" << totalBytes;
    });
    connect(api, &SafeApi::pushFileComplete, [=, this](ulong id, SafeFile fileInfo) {
        qDebug() << "New file uploaded:" << fileInfo.name;
        finishTransfer(path);
    });
    connect(api, &SafeApi::errorRaised, [=](ulong id, quint16 code, QString text){
        qWarning() << "Error uploading:" << text << "(" << code << ")";
        finishTransfer(path);
    });

    this->activeTransfers[path] = api;
    api->pushFile(dir_id, path, info.fileName(), true);
}

void SafeDaemon::queueDownloadFile(const QString &id, const QFileInfo &info)
{
    QTimer *timer = new QTimer(this);
    QString path(info.filePath());

    timer->setInterval(2000);
    timer->setSingleShot(true);
    timer->setTimerType(Qt::VeryCoarseTimer);

    bool active = this->activeTransfers.contains(path);
    if(active) {
        finishTransfer(path);
    }

    connect(timer, &QTimer::timeout, [=](){
        downloadFile(id, info);
    });

    bool queued = this->pendingTransfers.contains(path);
    if(queued) {
        this->pendingTransfers[path]->stop();
        this->pendingTransfers.take(path)->deleteLater();
    }
    this->pendingTransfers.insert(path, timer);
    timer->start();
}

void SafeDaemon::downloadFile(const QString &id, const QFileInfo &info)
{
    QString path(info.filePath());
    auto api = this->apiFactory->newApi();

    connect(api, &SafeApi::pullFileProgress, [=](ulong id, ulong bytes, ulong totalBytes){
        qDebug() << "D/Progress:" << bytes << "/" << totalBytes;
    });
    connect(api, &SafeApi::pullFileComplete, [=, this](ulong id) {
        qDebug() << "File downloaded:" << path;
        finishTransfer(path);
        QString file_id = this->remoteStateDb->getFileId(relativeFilePath(info));
        this->localStateDb->insertFile(relativePath(info), relativeFilePath(info),
                                       info.fileName(), getMtime(info), makeHash(info), file_id);
    });
    connect(api, &SafeApi::errorRaised, [=](ulong id, quint16 code, QString text){
        qWarning() << "Error downloading:" << text << "(" << code << ")";
        finishTransfer(path);
    });

    this->activeTransfers[path] = api;
    api->pullFile(id, path);
}

void SafeDaemon::remoteRemoveFile(const QFileInfo &info)
{
    QString path(info.filePath());
    QString id(this->remoteStateDb->getFileId(relativeFilePath(info)));
    if(id.isEmpty()) {
        qWarning() << "File" << relativeFilePath(info) << "isn't exists in the remote index";
        return;
    }

    auto api = this->apiFactory->newApi();
    connect(api, &SafeApi::removeFileComplete, [=, this](ulong id){
        qDebug() << "Remote file deleted" << path;
        finishTransfer(path);
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error deleting:" << text << "(" << code << ")";
        finishTransfer(path);
    });

    this->activeTransfers.insert(path, api);
    api->removeFile(id, true);
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

void SafeDaemon::updateDirHash(const QDir &dir)
{
    this->localStateDb->updateDirHash(relativeFilePath(
                                          QFileInfo(dir.absolutePath())));
}

ulong SafeDaemon::getMtime(const QFileInfo &info)
{
    return info.lastModified().toTime_t();
}

void SafeDaemon::fullIndex(const QDir &dir)
{
    qDebug() << "Doing full local index";
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
            stats.dirs++;
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

        // index root
        if(root)
            remoteStateDb->insertDir(tree, tree, 0, root_info.value("id").toString());

        foreach(SafeFile file, files) {
            if(file.is_trash) {
                continue;
            }
            // index file
            remoteStateDb->insertFile(tree, root ? file.name : (tree + QDir::separator() + file.name),
                                      file.name, file.mtime, file.chksum, file.id);
        }

        foreach(SafeDir dir, dirs) {
            if(dir.is_trash || !dir.special_dir.isEmpty()) {
                continue;
            }
            ++counter;
            // index dir
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
        qWarning() << "Error remote indexing:" << text << "(" << code << ")";
        --counter;
        if(counter < 1)
            loop.exit();

    });

    ++counter;
    api->listDir(fetchDirId("/"));
    loop.exec();

    qDebug() << "Finished remote indexing";
}

void SafeDaemon::checkIndex(const QDir &dir)
{

}

QString SafeDaemon::relativeFilePath(const QFileInfo &info)
{
    QString relative = QDir(getFilesystemPath()).relativeFilePath(info.filePath());
    return relative.isEmpty() ? "/" : relative;
}

QString SafeDaemon::fetchDirId(const QString &path)
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
