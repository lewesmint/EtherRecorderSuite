# Configuration System Design

## Overview
The configuration system provides a flexible, INI-style configuration mechanism that supports runtime modifications and platform-specific settings.

## Key Features
- INI-style syntax
- Runtime modification support
- Thread-specific configurations
- Hierarchical override system
- Default fallback values

## Configuration Structure

### File Format
```ini
[section]
key=value  ; values are trimmed
# Comments use ; or #
```

### Core Sections

#### Logger Configuration
```ini
[logger]
log_level=DEBUG          # TRACE, DEBUG, INFO, NOTICE, WARN, ERROR, CRITICAL, FATAL
log_destination=both     # file, console, or both
log_file_path=logs      # Base directory for all log files
log_file_size=10485760  # Size in bytes before rotation
timestamp_granularity=microsecond  # nanosecond, microsecond, millisecond, etc.
```

#### Network Configuration
```ini
[network]
mode=server             # server, client, or both
client.server_hostname=localhost
client.server_port=4200
client.protocol=tcp
server.server_port=4200
```

#### Debug Configuration
```ini
[debug]
suppress_threads=client,server  # Comma-separated list of threads to suppress
trace_on=off                   # Enable/disable trace logging
```

## Implementation Details

### Configuration Loading
1. Load default values
2. Parse configuration file
3. Apply environment variables
4. Apply command-line arguments

### Thread-Specific Configuration
- Each thread can have its own logging configuration
- Format: `threadname.setting=value`
- Example: `client.log_file_name=client.log`

### Value Types
- Boolean: true/false, on/off, yes/no
- Integer: Decimal or hexadecimal (0x prefix)
- String: Plain text, trimmed
- Enum: Predefined value sets

## Usage Example

```c
// Initialize configuration
ConfigHandle config = config_init("config.ini");

// Read values with type safety
const char* log_level = config_get_string(config, "logger", "log_level", "INFO");
uint32_t port = config_get_uint32(config, "network", "server.server_port", 8080);
bool debug = config_get_bool(config, "debug", "trace_on", false);

// Thread-specific configuration
const char* thread_log = config_get_thread_string(config, "client", "log_file_name", "default.log");
```

## Error Handling
- Invalid values trigger warnings and use defaults
- Missing required values can trigger fatal errors
- Type mismatches are logged and default values used
- Malformed sections or keys are ignored with warnings

## Best Practices
1. Always provide default values in code
2. Use thread-specific configurations sparingly
3. Document all configuration options
4. Validate critical settings at startup
5. Log configuration changes