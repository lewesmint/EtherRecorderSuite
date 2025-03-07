# Platform Layer

This library provides a platform abstraction layer for the Claude Ether project.

## Directory Structure

```
platform_layer/
├── inc/                # Public platform-agnostic headers
│   ├── platform_atomic.h
│   ├── platform_console.h
│   ├── platform_defs.h
│   ├── platform_error.h
│   ├── platform_mutex.h
│   ├── platform_path.h
│   ├── platform_random.h
│   ├── platform_signal.h
│   ├── platform_sockets.h
│   ├── platform_string.h
│   ├── platform_threads.h
│   └── platform_time.h
├── windows/
│   ├── inc/          # Windows-specific private headers
│   └── src/          # Windows implementations
├── posix/
│   ├── inc/          # Common POSIX private headers
│   └── src/          # Common POSIX implementations
└── macos/
    ├── inc/          # macOS-specific private headers
    └── src/          # macOS-specific implementations or POSIX overrides
```

## Design Principles

1. Public headers in `inc/` are platform-agnostic and form the stable API
2. Platform-specific implementations are isolated in their respective directories
3. POSIX code serves as the base for Unix-like systems
4. macOS can use common POSIX code or provide optimized implementations
5. Private headers in platform directories' `inc/` contain platform-specific internal definitions

## Migration from claude_ether_original

The original `platform_utils.h` functionality has been split into specialized headers:

### New Headers

1. `platform_random.h`
   - Cryptographically secure RNG
   - Multiple output types (bytes, uint32, uint64, double)
   - Proper initialization and cleanup
   - Self-test capability

2. `platform_string.h`
   - Safe string operations with size limits
   - Thread-safe tokenization
   - Case-insensitive operations
   - Modern string utilities (starts_with, ends_with)
   - String transformation (upper/lower/trim)

3. `platform_path.h`
   - Platform-agnostic path handling
   - Path normalization and component extraction
   - Directory operations
   - Absolute/relative path conversion

4. `platform_console.h`
   - Console control and formatting
   - Color and text attributes
   - Cursor control
   - Input mode management

5. `platform_signal.h`
   - Signal handling and registration
   - Shutdown handler support
   - Signal blocking controls

### Migration Guide

1. Replace `platform_utils.h` includes with specific headers
2. Update random number generation:
   ```c
   // Old
   uint32_t rand = platform_random();
   
   // New
   platform_random_init();
   uint32_t rand;
   platform_random_bytes(&rand, sizeof(rand));
   ```

3. Update string operations:
   ```c
   // Old
   str_cmp_nocase(s1, s2);
   platform_strtok(str, delim, &saveptr);
   
   // New
   platform_strcasecmp(s1, s2);
   platform_strtok(str, delim, &saveptr);
   ```

4. Update path handling:
   ```c
   // Old
   resolve_full_path(filename, full_path, size);
   
   // New
   platform_path_resolve(filename, full_path, size);
   ```

5. Update signal handling:
   ```c
   // Old
   platform_register_shutdown_handler(callback);
   
   // New
   platform_register_shutdown_handler(callback);
   // Now in platform_signal.h
   ```

## Supported Platforms

- Windows
- POSIX-compliant systems
- macOS (with platform-specific optimizations)
