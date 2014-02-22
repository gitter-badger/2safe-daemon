#include "safedaemon.h"

SafeDaemon::SafeDaemon(const QString &name, QObject *parent = 0) : QLocalServer(parent) {
    this->listen(name);
    this->settings = new QSettings(ORG_NAME, APPLICATION_NAME, this);
    this->api = new SafeApi(API_HOST);
    //this->authUser();
    this->authUserComplete();
}

SafeDaemon::~SafeDaemon() {
    delete this->api;
}

void SafeDaemon::authUser() {
    QString login = this->settings->value("login", "").toString();
    QString password = this->settings->value("password", "").toString();

    if(login.length() < 1 || password.length() < 1) return;

    this->connect(this->api, &SafeApi::authUserComplete, &SafeDaemon::authUserComplete);
    this->api->authUser(login, password);
}

void SafeDaemon::authUserComplete() {
    this->filesystem = new SafeFileSystem(getFilesystemPath(), STATE_DATABASE, true, this);
}

void SafeDaemon::incomingConnection(quintptr descriptor) {
    QLocalSocket *socket = new QLocalSocket(this);
    connect(socket, &QLocalSocket::readyRead, this, &SafeDaemon::readClient);
    connect(socket, &QLocalSocket::disconnected, this, &SafeDaemon::discardClient);
    socket->setSocketDescriptor(descriptor);
}

void SafeDaemon::readClient() {
    QLocalSocket* socket = (QLocalSocket*)sender();
    if (socket->canReadLine()) {
        QJsonParseError error;
        QJsonDocument requestData = QJsonDocument::fromJson(socket->readLine(), &error);

        if (error.error == QJsonParseError::NoError && requestData.isObject()) {
            QJsonObject requestJson = requestData.object();
            QString requestType = requestJson["type"].toString();

            if (requestType == "set_settings") {
                this->setSettings(requestJson["args"].toObject());
            } else if (requestType == "get_settings") {
                QJsonObject responseObject = this->getSettings(requestJson["fields"].toArray());
                QJsonDocument responseJson;
                responseJson.setObject(responseObject);
                QTextStream stream(socket);
                stream.autoDetectUnicode();
                stream << responseJson.toJson();
                socket->close();

                if (socket->state() == QTcpSocket::UnconnectedState) {
                    delete socket;
                }
            }
        }
    }
}

void SafeDaemon::discardClient() {
    QLocalSocket *socket = (QLocalSocket*)sender();
    socket->deleteLater();
}

void SafeDaemon::setSettings(const QJsonObject &requestArgs) {
    for (QJsonObject::ConstIterator i = requestArgs.begin(); i != requestArgs.end(); ++i) {
        this->settings->setValue(i.key(), i.value().toString());
    }
}

QJsonObject SafeDaemon::getSettings(const QJsonArray &requestFields) {
    QJsonObject result, values;
    for (QJsonArray::ConstIterator i = requestFields.begin(); i != requestFields.end(); ++i) {
        values.insert((*i).toString(), this->settings->value((*i).toString()).toString());
    }
    result.insert("type", QJsonValue(QString("settings")));
    result.insert("values", values);
    return result;
}

QString SafeDaemon::getFilesystemPath()
{
    QString root = this->settings->value("root_name", DEFAULT_ROOT_NAME).toString();
    return QDir::cleanPath(QDir::homePath() + QDir::separator() + root);
}
