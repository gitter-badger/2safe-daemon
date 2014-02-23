#include "safedaemon.h"

SafeDaemon::SafeDaemon(QObject *parent) : QObject(parent) {
    this->settings = new QSettings(ORG_NAME, APP_NAME, this);
    this->api = new SafeApi(API_HOST);
    this->server = new QLocalServer(this);

    connect(server, &QLocalServer::newConnection, this, &SafeDaemon::handleClientConnection);
    this->bindServer(this->server,
                     QDir::homePath() +
                     QDir::separator() + SAFE_DIR +
                     QDir::separator() + SOCKET_FILE);

    this->authUser();
}

SafeDaemon::~SafeDaemon() {
    delete this->api;
}

bool SafeDaemon::isListening() {
    return this->server->isListening();
}

QString SafeDaemon::socketPath() {
    return this->server->fullServerName();
}

void SafeDaemon::authUser() {
    QString login = this->settings->value("login", "").toString();
    QString password = this->settings->value("password", "").toString();

    if (login.length() < 1 || password.length() < 1) {
        return;
    }

    connect(this->api, &SafeApi::authUserComplete, this, &SafeDaemon::authUserComplete);
    this->api->authUser(login, password);
}

void SafeDaemon::authUserComplete() {
    qDebug() << "Authorization completed";

    this->filesystem = new SafeFileSystem(getFilesystemPath(), STATE_DATABASE, this);
    connect(this->filesystem, &SafeFileSystem::fileAdded, this, &SafeDaemon::fileAdded);
    connect(this->filesystem, &SafeFileSystem::fileChanged, this, &SafeDaemon::fileChanged);
    this->filesystem->startWatching();
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
    } else {
        qWarning() << "Got message of unknown type:" << type;
    }

    socket->close();
}

void SafeDaemon::fileAdded(const QFileInfo &info, const QString &hash, const uint &updatedAt) {
    qDebug() << "Uploading file" << info.filePath();

    uint id = this->api->pushFile("227930033757", info.filePath(), info.fileName());
}

void SafeDaemon::fileChanged(const QFileInfo &info, const QString &hash, const uint &updatedAt) {

}
