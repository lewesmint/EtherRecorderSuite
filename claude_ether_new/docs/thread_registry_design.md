# Thread Registry Design

## Overview
The thread registry system provides thread-safe lifecycle management and status tracking for system threads. It ensures reliable thread creation, monitoring, and cleanup while preventing resource leaks and race conditions.

## Core Components

### Public Interfaces

#### ThreadRegistry
```c
typedef struct ThreadRegistry {
    ThreadRegistryEntry* head;     
    RWLock registry_lock;          // Read-write lock for better concurrency
    uint32_t count;               
    EventNotifier* state_notifier; // For state change notifications
    const char* owner_label;       // Added owner label
} ThreadRegistry;
```

#### ThreadStatus
```c
typedef struct ThreadStatus {
    const ThreadConfig* config;
    PlatformThreadHandle handle;   
    PlatformThreadHandle master_handle;  // Added: Reference to group master
    atomic_int state;              // Atomic state for thread-safe updates
    bool auto_cleanup;             
    ThreadCleanupHandler cleanup;   // Cleanup strategy
    const char* owner_label;       // Added owner label
} ThreadStatus;
```

#### Error Codes
```c
typedef enum ThreadRegistryError {
    THREAD_REG_SUCCESS = 0,
    THREAD_REG_INVALID_ARGS,
    THREAD_REG_DUPLICATE_THREAD,
    THREAD_REG_CREATION_FAILED,
    THREAD_REG_REGISTRATION_FAILED,
    THREAD_REG_LOCK_ERROR,
    THREAD_REG_CLEANUP_ERROR,
    THREAD_REG_INVALID_STATE_TRANSITION
} ThreadRegistryError;
```

### Internal Structures

#### ThreadRegistryEntry
```c
typedef struct ThreadRegistryEntry {
    ThreadStatus status;
    RWLock entry_lock;            // Fine-grained locking
    struct ThreadRegistryEntry* next;
    uint64_t creation_timestamp;
    uint32_t state_change_count;
    ThreadStatistics stats;
} ThreadRegistryEntry;
```

## Thread Lifecycle Management

### State Machine
```
                    +------------+
                    |  CREATED   |
                    +------------+
                          |
                          v
    +--------+     +------------+     +-----------+
    | FAILED |<--->|  RUNNING   |---->| STOPPING  |
    +--------+     +------------+     +-----------+
                          |                 |
                          v                 v
                    +------------+    +------------+
                    | SUSPENDED |    |TERMINATED  |
                    +------------+    +------------+
```

### State Transitions
- Valid transitions enforced through `thread_registry_validate_transition()`
- State changes atomic and thread-safe
- Notifications sent to registered observers

### Thread Registration Flow
```c
ThreadRegistryError thread_registry_register(
    ThreadRegistry* registry, 
    const ThreadConfig* config,
    PlatformThreadHandle handle,
    bool auto_cleanup
) {
    // Input validation
    if (!validate_registration_params(registry, config, handle)) {
        return THREAD_REG_INVALID_ARGS;
    }

    // Acquire write lock
    if (!registry_write_lock(registry)) {
        return THREAD_REG_LOCK_ERROR;
    }

    // Check for duplicates
    if (find_existing_thread(registry, config->label)) {
        registry_write_unlock(registry);
        return THREAD_REG_DUPLICATE_THREAD;
    }

    // Create and initialize entry
    ThreadRegistryError err = create_registry_entry(registry, config, handle);
    
    registry_write_unlock(registry);
    return err;
}
```

### Thread State Reporting
- Threads self-report state changes via `thread_registry_report_state()`
- State changes validated against state machine
- Observers notified of state changes
- Statistics updated atomically

## Synchronization Model

### Lock Hierarchy
1. Registry-level RWLock for structural changes
2. Entry-level RWLock for status updates
3. Atomic operations for state changes

### Read Operations
- Multiple readers allowed simultaneously
- State queries use atomic reads
- Statistics collection uses entry-level read locks

### Write Operations
- Registration/deregistration uses registry write lock
- Status updates use entry write lock
- State changes use atomic operations
- Group membership changes through master handle updates

## Error Handling and Recovery

### Error Recovery Procedures
1. Registration Failure:
   - Clean up partial entry
   - Release locks
   - Notify failure
   - Return error code

2. State Change Failure:
   - Maintain previous state
   - Log error
   - Notify observers
   - Allow retry

3. Cleanup Failure:
   - Attempt partial cleanup
   - Mark for manual intervention
   - Log detailed error

### Resource Management

#### Memory Management
- All allocations tracked
- Cleanup handlers registered
- Resource limits enforced
- Memory leaks prevented through cleanup chain

#### Handle Management
- Platform handles wrapped
- Reference counting for shared handles
- Automatic handle cleanup
- Handle validation on operations

## Thread Cleanup

### Auto Cleanup Mode
1. Thread signals termination
2. Registry detects state change
3. Cleanup handler invoked
4. Resources released
5. Entry removed
6. Observers notified

### Manual Cleanup Mode
1. Application initiates cleanup
2. Registry validates state
3. Resources released
4. Entry removed
4. Cleanup confirmed

## Performance Considerations

### Lock Optimization
- Read-heavy operations optimized
- Fine-grained locking used
- Lock contention minimized
- Lock-free operations where possible

### Scalability
- O(1) state changes
- O(log n) thread lookup
- O(n) cleanup operations
- Configurable thread limits

## Implementation Requirements

### Thread Safety
- All public APIs thread-safe
- Internal operations properly synchronized
- No deadlock scenarios
- Race conditions prevented

### Resource Limits
- Maximum threads configurable
- Memory usage bounded
- Handle limits enforced
- Queue sizes limited

### Platform Requirements
- Atomic operations support
- Thread local storage
- Read-write lock support
- Event notification system

## Validation

### Input Validation
- Thread labels: non-null, unique, max length
- Handles: non-null, valid platform handle
- States: valid enum values
- Configuration: complete and valid

### State Validation
- Transitions follow state machine
- No invalid state combinations
- State history maintained
- Statistics validated

## Monitoring and Debugging

### Statistics Collection
- Thread creation time
- State change counts
- Error counts
- Resource usage

### Debugging Support
- State change history
- Error tracking
- Resource monitoring
- Performance metrics

# Message Queue Design

## Overview
The message queue system provides a thread-safe mechanism for inter-thread communication. It supports multiple producers and consumers, ensuring reliable message delivery and preventing race conditions.

## Core Components

### Public Interfaces

#### MessageQueue
```c
typedef struct MessageQueue {
    Message_T* entries;
    volatile int32_t head;
    volatile int32_t tail;
    int32_t max_size;
    PlatformEvent_T not_empty_event;
    PlatformEvent_T not_full_event;
    const char* owner_label;         // Thread label that owns this queue
} MessageQueue_T;
```

### Queue Management
- Queue ownership tracked via owner_label
- Queue initialized in owner thread's context
- Debug logging for queue state changes
- Error messages include queue owner information

## Queue Operations

### Enqueue Operation
```c
bool message_queue_enqueue(MessageQueue_T* queue, Message_T* message) {
    // Implementation details
}
```

### Dequeue Operation
```c
bool message_queue_dequeue(MessageQueue_T* queue, Message_T* message) {
    // Implementation details
}
```

## Synchronization Model

### Lock Hierarchy
1. Queue-level lock for structural changes
2. Atomic operations for head and tail updates

### Read Operations
- Multiple readers allowed simultaneously
- Message retrieval uses atomic reads
- Queue state queries use atomic reads

### Write Operations
- Enqueue/dequeue operations use queue lock
- Head and tail updates use atomic operations

## Error Handling and Recovery

### Error Recovery Procedures
1. Enqueue Failure:
   - Return false if queue is full
   - Log error message with queue owner information

2. Dequeue Failure:
   - Return false if queue is empty
   - Log error message with queue owner information

### Resource Management

#### Memory Management
- All allocations tracked
- Cleanup handlers registered
- Resource limits enforced
- Memory leaks prevented through cleanup chain

#### Handle Management
- Platform handles wrapped
- Reference counting for shared handles
- Automatic handle cleanup
- Handle validation on operations

## Performance Considerations

### Lock Optimization
- Read-heavy operations optimized
- Fine-grained locking used
- Lock contention minimized
- Lock-free operations where possible

### Scalability
- O(1) enqueue and dequeue operations
- O(1) space complexity
- Configurable queue sizes

## Implementation Requirements

### Thread Safety
- All public APIs thread-safe
- Internal operations properly synchronized
- No deadlock scenarios
- Race conditions prevented

### Resource Limits
- Maximum queue size configurable
- Memory usage bounded
- Handle limits enforced
- Queue sizes limited

### Platform Requirements
- Atomic operations support
- Thread local storage
- Read-write lock support
- Event notification system

## Validation

### Input Validation
- Queue size: positive integer
- Messages: non-null, valid message format

### State Validation
- Queue state maintained
- No invalid state combinations
- State history maintained
- Statistics validated

## Monitoring and Debugging

### Statistics Collection
- Queue size
- Enqueue and dequeue counts
- Error counts
- Resource usage

### Debugging Support
- Queue state change history
- Error tracking
- Resource monitoring
- Performance metrics
