# Migration Guide from claude_ether_original

This document details the migration path from the original `platform_utils.h` to the new specialized platform headers.

## Overview of Changes

The monolithic `platform_utils.h` has been split into specialized headers for better organization and functionality:

| Original Location | New Location | Key Improvements |
|------------------|--------------|------------------|
| `platform_random()` | `platform_random.h` | Cryptographic security, multiple types, proper initialization |
| `str_cmp_nocase()` | `platform_string.h` | Thread-safety, comprehensive string operations |
| `resolve_full_path()` | `platform_path.h` | Platform-agnostic paths, better component handling |
| `init_console()` | `platform_console.h` | Full console control, color support |
| `platform_register_shutdown_handler()` | `platform_signal.h` | Complete signal handling |

## Step-by-Step Migration

### 1. Random Number Generation

```c
// OLD code
#include "platform_utils.h"

uint32_t value = platform_random();
uint32_t range = platform_random_range(1, 100);

// NEW code
#include "platform_random.h"

platform_random_init();
uint32_t value;
platform_random_bytes(&value, sizeof(value));
uint32_t range = platform_random_range(1, 100);
// When done:
platform_random_cleanup();
```

### 2. String Operations

```c
// OLD code
#include "platform_utils.h"

char *token, *saveptr;
token = platform_strtok(str, " ", &saveptr);
if (str_cmp_nocase(str1, str2) == 0) {
    // strings match
}

// NEW code
#include "platform_string.h"

char *token, *saveptr;
token = platform_strtok(str, " ", &saveptr);
if (platform_strcasecmp(str1, str2) == 0) {
    // strings match
}
```

### 3. Path Handling

```c
// OLD code
#include "platform_utils.h"

char path[MAX_PATH_LEN];
resolve_full_path("./file.txt", path, sizeof(path));
sanitise_path(path);
create_directories(path);

// NEW code
#include "platform_path.h"

char path[PLATFORM_MAX_PATH];
platform_path_resolve("./file.txt", path, sizeof(path));
platform_path_normalize(path);
platform_path_create_directories(path);
```

### 4. Console Operations

```c
// OLD code
#include "platform_utils.h"

init_console();

// NEW code
#include "platform_console.h"

platform_console_init();
// New capabilities:
platform_console_set_color(PLATFORM_CONSOLE_COLOR_GREEN);
platform_console_clear_screen();
```

### 5. Signal Handling

```c
// OLD code
#include "platform_utils.h"

platform_register_shutdown_handler(on_shutdown);

// NEW code
#include "platform_signal.h"

platform_register_shutdown_handler(on_shutdown);
platform_register_signal_handler(PLATFORM_SIGNAL_INT, on_interrupt);
```

### 7. Error Code Migration

#### Overview
The original platform defines a more comprehensive set of error codes that need to be preserved in the new platform layer.

#### Changes Required

1. Update `platform_layer/inc/platform_error.h` to include all error codes:

```c
typedef enum PlatformErrorCode {
    // General errors (0-99)
    PLATFORM_ERROR_SUCCESS = 0,
    PLATFORM_ERROR_UNKNOWN = 1,
    PLATFORM_ERROR_INVALID_ARGUMENT = 2,
    PLATFORM_ERROR_NOT_IMPLEMENTED = 3,
    PLATFORM_ERROR_NOT_SUPPORTED = 4,
    PLATFORM_ERROR_PERMISSION_DENIED = 5,
    PLATFORM_ERROR_TIMEOUT = 6,

    // Socket errors (100-199)
    PLATFORM_ERROR_SOCKET_CREATE = 100,
    PLATFORM_ERROR_SOCKET_BIND = 101,
    PLATFORM_ERROR_SOCKET_CONNECT = 102,
    PLATFORM_ERROR_SOCKET_LISTEN = 103,
    PLATFORM_ERROR_SOCKET_ACCEPT = 104,
    PLATFORM_ERROR_SOCKET_SEND = 105,
    PLATFORM_ERROR_SOCKET_RECEIVE = 106,
    PLATFORM_ERROR_SOCKET_CLOSED = 107,
    PLATFORM_ERROR_HOST_NOT_FOUND = 108,

    // Thread errors (200-299)
    PLATFORM_ERROR_THREAD_CREATE = 200,
    PLATFORM_ERROR_THREAD_JOIN = 201,
    PLATFORM_ERROR_THREAD_DETACH = 202,
    PLATFORM_ERROR_MUTEX_INIT = 203,
    PLATFORM_ERROR_MUTEX_LOCK = 204,
    PLATFORM_ERROR_MUTEX_UNLOCK = 205,
    PLATFORM_ERROR_CONDITION_INIT = 206,
    PLATFORM_ERROR_CONDITION_WAIT = 207,
    PLATFORM_ERROR_CONDITION_SIGNAL = 208,

    // File system errors (300-399)
    PLATFORM_ERROR_FILE_NOT_FOUND = 300,
    PLATFORM_ERROR_FILE_EXISTS = 301,
    PLATFORM_ERROR_FILE_ACCESS = 302,
    PLATFORM_ERROR_DIRECTORY_NOT_FOUND = 303,
    PLATFORM_ERROR_DIRECTORY_EXISTS = 304,
    PLATFORM_ERROR_DIRECTORY_ACCESS = 305,

    // Memory errors (400-499)
    PLATFORM_ERROR_MEMORY_ALLOC = 400,
    PLATFORM_ERROR_MEMORY_FREE = 401,
    PLATFORM_ERROR_MEMORY_ACCESS = 402
} PlatformErrorCode;
```

2. Update error domains to match:
```c
typedef enum PlatformErrorDomain {
    PLATFORM_ERROR_DOMAIN_GENERAL,
    PLATFORM_ERROR_DOMAIN_SOCKET,
    PLATFORM_ERROR_DOMAIN_THREAD,
    PLATFORM_ERROR_DOMAIN_FILE,
    PLATFORM_ERROR_DOMAIN_MEMORY
} PlatformErrorDomain;
```

#### Migration Steps

1. Copy all error codes from original platform
2. Assign explicit numeric values to maintain ABI compatibility
3. Group errors by domain using ranges (0-99, 100-199, etc.)
4. Update all error mapping functions in platform-specific implementations
5. Update error message functions to handle all error codes
6. Update documentation to reflect complete error set

#### Verification

1. Check all error codes are mapped correctly
2. Verify error messages are meaningful
3. Test error code conversion from system errors
4. Ensure backward compatibility with existing code
5. Update unit tests to cover all error codes

#### Note
The explicit numbering scheme allows for future additions while maintaining backward compatibility and provides clear domain separation.

## Removed Functions

The following functions have been removed without direct replacements:
- `get_current_time()` - Use `platform_time.h` instead
- `sleep_ms()` - Use `platform_time.h` instead
- `stream_print()` - Use standard C functions instead

## Verification Steps

1. Remove includes of `platform_utils.h`
2. Add includes for specific platform headers
3. Update function calls to use new APIs
4. Add initialization/cleanup calls where required
5. Test thoroughly on all target platforms

## Getting Help

If you encounter issues during migration:
1. Check the header files for detailed documentation
2. Review the examples in the test directory
3. File an issue in the project repository

```

Would you like me to:
1. Add more migration examples?
2. Create example test cases?
3. Document additional platform-specific considerations?
