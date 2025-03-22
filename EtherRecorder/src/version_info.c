#include "version_info.h"

#include <stdio.h>

#include "utils.h"

void print_version_info(void) {
    stream_print(stdout, "\n");
    stream_print(stdout, "----------------------------------------\n");
    stream_print(stdout, "Claude Ether v%d.%d.%d\n", 
                APP_VERSION_MAJOR, 
                APP_VERSION_MINOR, 
                APP_VERSION_PATCH);
    stream_print(stdout, "Build Type: %s\n", BUILD_TYPE);
    stream_print(stdout, "Built on: %s at %s\n", BUILD_DATE, BUILD_TIME);
    stream_print(stdout, "----------------------------------------\n\n");
}
