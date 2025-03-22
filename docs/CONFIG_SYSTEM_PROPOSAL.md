# Configuration System Redesign Proposal

## Overview
This proposal outlines a redesign of the configuration system to provide:
- Centralized configuration management
- Type-safe configuration handling
- Runtime configuration updates
- Validation mechanisms
- Consistent configuration loading patterns
- Command interface integration

## Current Issues
1. Configuration loading is scattered across different components
2. No standardized way to update configurations at runtime
3. Validation is inconsistent
4. Configuration storage is mixed with component logic
5. No centralized way to save/load configurations

## Proposed Design

### 1. Configuration Registry
Central system for managing all component configurations:

```c
typedef struct {
    const char* name;           // Config identifier
    void* config;              // Pointer to config struct
    ConfigCreateFunc create;    // Creates default config
    ConfigDestroyFunc destroy;  // Cleanup handler
    ConfigUpdateFunc update;    // Updates single setting
    ConfigValidateFunc validate; // Validates entire config
} ConfigEntry;
```

### 2. Component Configuration Pattern
Each component defines:
- Configuration structure
- Default values
- Creation/destruction handlers
- Update handlers
- Validation rules

Example for File Reader:
```c
// Configuration structure
typedef struct {
    FileReadMode read_mode;
    size_t chunk_size;
    uint32_t chunk_delay_ms;
    uint32_t reload_delay_ms;
    uint32_t queue_timeout_ms;
    size_t max_queue_size;
    bool block_when_full;
    bool log_progress;
    uint32_t progress_interval_ms;
} FileReaderConfig;

// Registration function
void file_reader_config_register(void);
```

### 3. Configuration Lifecycle

1. **Initialization**
   - Registry initialization
   - Component config registration
   - Default config creation
   - Initial config load from file

2. **Runtime**
   - Config access through registry
   - Dynamic updates via command interface
   - Validation on updates
   - Optional persistence of changes

3. **Shutdown**
   - Optional config saving
   - Cleanup of config resources

### 4. Command Interface Integration

New commands:
```
config_update <config_name> <section> <key> <value>
config_save <filename>
config_load <filename>
config_list
config_get <config_name>
```

### 5. Implementation Plan

#### Phase 1: Core Infrastructure
1. Implement configuration registry
2. Create base configuration interfaces
3. Add validation framework
4. Implement persistence layer

#### Phase 2: Component Migration
1. Update file reader configuration
2. Update server configuration
3. Update client configuration
4. Update remaining components

#### Phase 3: Command Interface
1. Add configuration commands
2. Implement dynamic updates
3. Add configuration query capabilities

#### Phase 4: Testing & Documentation
1. Unit tests for registry
2. Integration tests for updates
3. Documentation updates
4. Migration guide

## API Examples

### Registry Usage
```c
// Initialization
config_registry_init();
file_reader_config_register();

// Get configuration
FileReaderConfig* config = config_registry_get("file_reader");

// Update configuration
config_registry_update("file_reader", "general", "chunk_size", "1024");

// Save all configurations
config_registry_save_file("config.ini");
```

### Component Implementation
```c
static void* file_reader_config_create(void) {
    FileReaderConfig* config = malloc(sizeof(FileReaderConfig));
    if (config) {
        *config = (FileReaderConfig){
            .read_mode = FILE_READ_ONCE,
            .chunk_size = 0,
            // ... other defaults ...
        };
    }
    return config;
}

static PlatformErrorCode file_reader_config_update(void* config, 
    const char* section, const char* key, const char* value) {
    FileReaderConfig* cfg = (FileReaderConfig*)config;
    
    if (strcmp(key, "read_mode") == 0) {
        cfg->read_mode = string_to_read_mode(value);
    }
    // ... handle other fields ...
    
    return PLATFORM_ERROR_SUCCESS;
}
```

## Benefits

1. **Centralization**
   - Single point of configuration management
   - Consistent handling across components
   - Simplified maintenance

2. **Type Safety**
   - Strong typing for configurations
   - Validation at update time
   - Clear error handling

3. **Runtime Updates**
   - Dynamic configuration changes
   - Command interface integration
   - Immediate feedback

4. **Maintainability**
   - Clear separation of concerns
   - Standardized patterns
   - Easy to add new configurations

5. **Flexibility**
   - Support for different storage formats
   - Easy to extend validation rules
   - Component-specific handling

## Migration Strategy

1. Create new configuration registry
2. Add component configurations one at a time
3. Update components to use new system
4. Remove old configuration code
5. Update documentation and tests

## Next Steps

1. Review and approve design
2. Create detailed implementation schedule
3. Identify priority components for migration
4. Create test plan
5. Begin implementation

## Notes

- Maintain backward compatibility during migration
- Consider adding configuration versioning
- Plan for future extensions (e.g., schema validation)
- Consider adding configuration change notifications
- Document all configuration options