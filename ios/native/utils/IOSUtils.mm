#include "IOSUtils.h"

#include <asl.h>
#include <unistd.h>

namespace massif {

    void IOSUtils::InitializeLog() {
        asl_add_log_file(NULL, STDERR_FILENO);
    }

    IOSUtils::IOSUtils() {
    }

}
