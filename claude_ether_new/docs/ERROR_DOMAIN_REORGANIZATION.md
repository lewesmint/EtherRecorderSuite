# Error Domain Reorganization Proposal

## Current Status
Currently, thread-related errors are spread across three domains:
- THREAD_REGISTRY_DOMAIN
- THREAD_STATUS_DOMAIN
- THREAD_RESULT_DOMAIN

Several errors in these domains actually relate to other subsystems (queues, logging, etc.).

## Proposed Changes

### New Domain Structure
```c
enum ErrorDomain {
    THREAD_REGISTRY_DOMAIN,    // Core registry operations
    THREAD_STATUS_DOMAIN,      // Thread lifecycle status
    THREAD_RESULT_DOMAIN,     // Thread execution results
    SYNC_DOMAIN,              // Synchronization primitives
    QUEUE_DOMAIN,             // Message queue operations
    RESOURCE_DOMAIN,          // Resource management
    LOGGER_DOMAIN,            // Logging subsystem
    CONFIG_DOMAIN,            // Configuration operations
    ERROR_DOMAIN_MAX
}
```

### Proposed Error Migration

#### To SYNC_DOMAIN
- `THREAD_REG_LOCK_ERROR`
- `THREAD_ERROR_MUTEX_ERROR`

#### To QUEUE_DOMAIN
- `THREAD_REG_QUEUE_FULL`
- `THREAD_REG_QUEUE_EMPTY`
- `THREAD_REG_QUEUE_ERROR`
- `THREAD_ERROR_QUEUE_ERROR`

#### To RESOURCE_DOMAIN
- `THREAD_REG_ALLOCATION_FAILED`

#### To LOGGER_DOMAIN
- `THREAD_ERROR_LOGGER_TIMEOUT`

#### To CONFIG_DOMAIN
- `THREAD_ERROR_CONFIG_ERROR`

## Implementation Considerations

### Function Return Types
Before implementing, need to review:
- All functions returning thread-related error codes
- Error handling patterns in calling code
- Error transformation/mapping functions
- Error logging and reporting systems

### Backward Compatibility
Consider:
- Impact on existing error handling code
- Need for error code mapping layer
- Version migration strategy
- Documentation updates

### Required Analysis
1. **Function Review**
   - Map all functions to their error return types
   - Identify error propagation chains
   - Document error handling patterns

2. **Impact Assessment**
   - Effect on existing code
   - Testing requirements
   - Documentation updates needed

3. **Migration Strategy**
   - Phased approach vs. direct replacement
   - Temporary compatibility layer needs
   - Version control considerations

### Next Steps
1. Create comprehensive list of affected functions
2. Document current error handling patterns
3. Create test cases covering error conditions
4. Design migration strategy
5. Create prototype implementation
6. Review with team

## Related Files to Review
- `thread_registry.h/c`
- `thread_group.h/c`
- `platform_error.h`
- Error handling utility functions
- Logging system integration
- Test suites

## Questions to Address
1. How to handle cross-domain errors?
2. Should we maintain error code ranges per domain?
3. Impact on error message formatting?
4. Effect on debugging and logging?
5. Performance implications of domain checks?

## Future Considerations
- Error aggregation across domains
- Domain-specific error handling policies
- Error recovery strategies
- Monitoring and metrics

## References
- Current error handling documentation
- Platform abstraction layer design
- Thread coordination design
- Logging system design

## Notes
This reorganization should be considered as part of a larger error handling improvement initiative. Implementation should wait until we have:
1. Complete function analysis
2. Test coverage metrics
3. Impact assessment
4. Team consensus on approach