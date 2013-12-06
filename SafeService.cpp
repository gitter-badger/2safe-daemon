#include "safeservice.h"

SafeService::SafeService(int argc, char **argv) : QtService<QCoreApplication>(argc, argv, "2 Safe Daemon")
{
    setServiceDescription("2 Safe Daemon");
}

void SafeService::start()
{
    this->daemon = new SafeDaemon(); // TODO should we delete it somewhere?
}

void SafeService::stop()
{

}
