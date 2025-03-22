# Platform Error Integration Proposal

## Current Situation

We have two separate error handling systems:
1. High-level application domains (Thread Registry, Status, Result)
2. Platform-level error handling (System, IO, Network, etc.)

## Integration Challenges

### 1. Domain Overlap
- Platform has THREAD domain while we have three thread-related domains
- Platform has RESOURCE domain which could encompass memory, IO, etc.
- Some platform errors might cross multiple application domains

### 2. Error Code Ranges
- Platform uses explicit numeric values for ABI compatibility
- Our error codes might conflict with platform error codes
- Need to maintain clear separation or mapping between layers

### 3. Error Handling Patterns
- Platform uses thread-local storage for error state
- Platform provides both error codes and messages
- Different error retrieval mechanisms between systems

## Proposed Integration Approaches

### Option 1: Hierarchical Integration
```c
enum ErrorDomain {
    // Platform-level domains (0-999)
    PLATFORM_SYSTEM_DOMAIN        = 0,
    PLATFORM_IO_DOMAIN           = 100,
    PLATFORM_NETWORK_DOMAIN      = 200,
    PLATFORM_THREAD_DOMAIN       = 300,
    PLATFORM_MEMORY_DOMAIN       = 400,
    PLATFORM_TIME_DOMAIN         = 500,
    PLATFORM_RESOURCE_DOMAIN     = 600,

    // Application-level domains (1000+)
    APP_THREAD_REGISTRY_DOMAIN   = 1000,
    APP_THREAD_STATUS_DOMAIN     = 1100,
    APP_THREAD_RESULT_DOMAIN     = 1200,
    APP_SYNC_DOMAIN             = 1300,
    APP_QUEUE_DOMAIN            = 1400,
    APP_LOGGER_DOMAIN           = 1500,
    APP_CONFIG_DOMAIN           = 1600,
}
```

### Option 2: Bridge Layer
Create a translation layer that maps between platform and application errors:
```c
typedef struct {
    ErrorDomain app_domain;
    int app_error;
    PlatformErrorDomain platform_domain;
    PlatformErrorCode platform_error;
} ErrorMapping;
```

### Option 3: Unified Error System
Merge both systems into a new comprehensive error handling system:
```c
typedef struct {
    uint16_t domain;      // Combined domain enum
    uint16_t layer;       // PLATFORM_LAYER or APP_LAYER
    int32_t code;         // Error code
    uint32_t system_code; // Original system error code
    char message[256];    // Error message
} UnifiedError;
```

## Implementation Considerations

### 1. Error Code Mapping
- Need clear rules for mapping platform errors to application domains
- Consider creating domain-specific mapping tables
- Handle cases where errors don't map cleanly

### 2. Error Retrieval
- Standardize error retrieval mechanism
- Consider keeping platform's thread-local storage
- Provide unified API for error information

### 3. Error Propagation
- Define rules for error propagation across layers
- Consider adding context/stack to error information
- Handle cross-domain error scenarios

## Required Changes

### Phase 1: Preparation
1. Audit all error codes and their usage
2. Document error handling patterns
3. Create comprehensive test suite
4. Design new error handling API

### Phase 2: Implementation
1. Create new error type definitions
2. Implement error mapping/translation
3. Update error handling functions
4. Add new error retrieval mechanisms

### Phase 3: Migration
1. Update existing code gradually
2. Maintain backward compatibility
3. Update documentation
4. Expand test coverage

## Questions to Address

1. How to handle platform-specific error information?
2. Should we maintain separate error stacks for each layer?
3. How to handle error translation performance?
4. Impact on existing error handling code?
5. How to maintain ABI compatibility?

## Next Steps

1. Review current error handling patterns
2. Choose integration approach
3. Create detailed implementation plan
4. Design new API
5. Create prototype
6. Review with team

## Note
This integration should be part of a larger error handling improvement initiative. 
Implementation should wait until we have:
1. Complete analysis of both error systems
2. Clear understanding of all use cases
3. Agreement on integration approach
4. Comprehensive test strategy