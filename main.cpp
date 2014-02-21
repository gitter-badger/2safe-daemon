#include "safedaemon.h"
#include "safeservice.h"
#include "safefilesystem.h"

int main(int argc, char *argv[]) {
    SafeService service(argc, argv);
    return service.exec();
}
