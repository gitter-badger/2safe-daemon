#include "safeapifactory.h"

SafeApiFactory::SafeApiFactory(QString host, QObject *parent) :
    QObject(parent)
{
    this->host = host;
}

SafeApi *SafeApiFactory::newApi()
{
    auto api = new SafeApi(this->host, this);

    if(QDateTime::currentDateTime().toTime_t() - this->sharedState.tokenTimestamp < TOKEN_LIFESPAN / 2) {
        api->setState(this->sharedState);
        return api;
    }

    if(this->authUser(this->login, this->password)) {
        return api;
    }

    return NULL;
}

bool SafeApiFactory::authUser(QString login, QString password)
{
    QEventLoop loop;
    auto api = new SafeApi(this->host);
    bool success;

    connect(api, &SafeApi::authUserComplete, [&](ulong id, QString user_id){
        qDebug() << "Authentication complete (user id:" << user_id << ")";
        this->sharedState = api->state();
        this->login = login;
        this->password = password;
        success = true;
        loop.exit();
    });
    connect(api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        this->sharedState.clear();
        api = NULL;
        qWarning() << "Authentication error:" << text;
        loop.exit();
    });

    api->authUser(login, password);
    loop.exec();
    return success;
}
