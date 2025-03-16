# Migration Guide

## Current Migration Status

### Completed
- Platform Layer split into specialized headers
- Error handling system
- Thread management and synchronization
- Console operations
- File system operations
- Random number generation
- String operations
- Socket operations

### In Progress
- Client class migration
- Command interpreter migration

## New Platform Layer Structure

### Core Headers
```c
platform_layer/inc/
├── platform_atomic.h    // Atomic operations
├── platform_console.h   // Console management
├── platform_error.h     // Error handling
├── platform_mutex.h     // Synchronization primitives
├── platform_path.h      // Path operations
├── platform_random.h    // Random number generation
├── platform_signal.h    // Signal handling
├── platform_string.h    // String operations
├── platform_threads.h   // Thread management
└── platform_time.h      // Time functions
```

### Migration Steps for Remaining Components

1. **Client Class Migration**
   - Move from monolithic to modular design
   - Update error handling
   - Implement new thread registry integration
   - Update socket operations

2. **Command Interpreter**
   - Split into modular components
   - Update string handling
   - Implement new error reporting
   - Update console interactions

## API Changes Reference

### String Operations
```c
// OLD
str_cmp_nocase(s1, s2);
platform_strtok(str, delim, &saveptr);

// NEW
platform_strcasecmp(s1, s2);
platform_strtok(str, delim, &saveptr);
```

### Path Operations
```c
// OLD
resolve_full_path(filename, full_path, size);
sanitise_path(path);

// NEW
platform_path_resolve(filename, full_path, size);
platform_path_normalize(path);
```

### Console Operations
```c
// OLD
init_console();

// NEW
platform_console_init();
platform_console_set_color(PLATFORM_CONSOLE_COLOR_GREEN);
platform_console_clear_screen();
```

### Error Handling
```c
// OLD
if (!platform_event_create(&event, true, false)) {
    // Error handling
}

// NEW
PlatformErrorCode result = platform_event_create(&event, true, false);
if (result != PLATFORM_ERROR_SUCCESS) {
    // Error handling with specific error code
}
```

## Verification Steps

1. Remove includes of legacy headers
2. Add includes for specific platform headers
3. Update function calls to use new APIs
4. Add proper error handling
5. Test thoroughly on all target platforms

## Getting Help

1. Check header files for detailed documentation
2. Review examples in test directory
3. File an issue in the project repository
