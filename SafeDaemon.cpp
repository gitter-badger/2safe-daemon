#include "safedaemon.h"

SafeDaemon::SafeDaemon(const QString &name, QObject *parent = 0) : QLocalServer(parent) {
    this->listen(name);
    this->settings = new QSettings("2safe", "2safe", this);
    //this->authUser();
    this->authUserComplete();
}

SafeDaemon::~SafeDaemon() {
    delete this->api;
}

void SafeDaemon::authUser() {
    if (this->settings->contains("login") && this->settings->contains("password")) {
        this->api = new SafeApi("https://api.2safe.com");
        connect(this->api, &SafeApi::authUserComplete, this, &SafeDaemon::authUserComplete);
        this->api->authUser(this->settings->value("login").toString(), this->settings->value("password").toString());
    }
}

void SafeDaemon::authUserComplete() {
    this->filesystem = new SafeFileSystem(QDir::cleanPath(QDir::homePath() + "/2safe"), "2safe.db", true, this);
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
