#include "SafeDaemon.h"
#include "SafeService.h"

int main(int argc, char *argv[])
{
    SafeService service(argc, argv);
    return service.exec();
}
