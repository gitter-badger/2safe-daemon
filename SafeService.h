#ifndef SAFESERVICE_H
#define SAFESERVICE_H

#include <QCoreApplication>
#include "qtservice.h"
#include "safedaemon.h"

class SafeService : public QtService<QCoreApplication> {
public:
    SafeService(int argc, char **argv);

protected:
    void start();
    void stop();

private:
    SafeDaemon *daemon;
};

#endif // SAFESERVICE_H
