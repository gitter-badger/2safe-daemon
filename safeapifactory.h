#ifndef SAFEAPIFACTORY_H
#define SAFEAPIFACTORY_H

#include <QObject>
#include <safeapi.h>
#include <safecommon.h>

class SafeApiFactory : public QObject
{
    Q_OBJECT
public:
    explicit SafeApiFactory(QString host, QObject *parent = 0);
    SafeApi* newApi();
    bool authUser(QString login, QString password);
    void setState(SafeApiState state){ this->sharedState = state; }
    void setLogin(QString login) { this->m_login = login; }
    void setPassword(QString password) { this->password = password; }
    QString login(){ return this->m_login; }

private:
    QString host;
    QString m_login;
    QString password;
    SafeApiState sharedState;

signals:

public slots:

};

#endif // SAFEAPIFACTORY_H
