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

1. **Platform Agnosticism**
   - Public headers in `inc/` form the stable API
   - No platform-specific types or defines in public headers
   - Consistent behavior across all supported platforms

2. **Error Handling**
   - Comprehensive error codes with domains
   - Detailed error information
   - Thread-safe error reporting
   - No error state persistence

3. **Resource Management**
   - RAII-style resource handling where possible
   - Explicit cleanup functions for all resources
   - Resource tracking and leak detection in debug builds

4. **Thread Safety**
   - All public APIs are thread-safe unless documented otherwise
   - Thread-local storage for state management
   - Atomic operations for shared state
   - Proper mutex handling and deadlock prevention

## Implementation Guidelines

### Windows Implementation
- Use modern Windows APIs (Windows 8+)
- Prefer UTF-16 internally, UTF-8 externally
- Use security-enhanced APIs
- Implement proper HANDLE management

### POSIX Implementation
- Target POSIX.1-2017 compliance
- Use modern POSIX APIs
- Proper file descriptor management
- Signal-safe implementations where required

### macOS Implementation
- Build on POSIX implementation
- Add platform-specific optimizations
- Support Apple Silicon and Intel
- Integrate with system frameworks when beneficial

## Build Configuration

### Required Tools
- CMake 3.10 or higher
- C17 compliant compiler
- Ninja build system (recommended)
- Python 3.x (for build scripts)

### Build Types
- Debug: Full debug info, assertions, leak checks
- Release: Optimized, minimal debug info
- RelWithDebInfo: Optimized with debug info

### Platform-Specific Features

#### Windows
- Control Flow Guard
- ASLR Support
- DEP Enforcement
- Safe Exception Handling

#### POSIX/macOS
- Position Independent Code
- Stack Protector
- Fortify Source
- Strict Aliasing Rules

## Testing

### Test Categories
1. Unit Tests
   - Per-component testing
   - Platform-specific edge cases
   - Error condition verification

2. Integration Tests
   - Cross-component interaction
   - Resource management
   - Thread coordination

3. Platform Tests
   - Platform-specific behavior
   - API compatibility
   - Performance benchmarks

### Running Tests
```bash
# Debug build and test
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build

# Release build with tests
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Documentation

### API Documentation
- Each header contains detailed documentation
- Example usage in comments
- Thread safety notes
- Error handling requirements

### Platform Notes
- Platform-specific limitations documented
- Performance characteristics
- Feature availability matrix
- Known issues and workarounds

## Version Control

### Branch Structure
- main: Stable releases
- develop: Integration branch
- feature/*: Feature development
- fix/*: Bug fixes

### Commit Guidelines
- Atomic commits
- Clear commit messages
- Reference issues
- Include tests for changes
