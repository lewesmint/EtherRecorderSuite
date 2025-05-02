#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory_monitor.h"
#include "platform_shared_memory.h"
#include "platform_error.h"
#include "platform_time.h"
#include "thread_registry.h"
#include "logger.h"

// Default configuration
static SharedMemoryMonitorConfig monitor_config = {
    .name = "TestSharedMemory",
    .size = 0,                  // Auto-detect size
    .interval_ms = 1000,        // Check every second
    .retry_interval_ms = 2000,  // Retry every 2 seconds
    .max_retries = 5,           // 5 retries by default
    .wait_indefinitely = true,  // Wait indefinitely by default
    .callback = NULL            // No default callback
};
