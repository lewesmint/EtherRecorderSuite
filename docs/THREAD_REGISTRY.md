# Thread Registry

## Overview
The thread registry provides centralized thread management, state tracking, and inter-thread communication through message queues.

## Core Features
- Thread lifecycle management
- State tracking and transitions
- Message queuing
- Resource cleanup
- Thread synchronization

## Thread States
```c
typedef enum {
    THREAD_STATE_UNKNOWN,
    THREAD_STATE_CREATED,
    THREAD_STATE_RUNNING,
    THREAD_STATE_STOPPING,
    THREAD_STATE_TERMINATED,
    THREAD_STATE_FAILED
} ThreadState;
```

## API Reference

### Initialization
```c
ThreadRegistryError init_global_thread_registry(void);
void thread_registry_cleanup(void);
```

### Thread Management
```c
ThreadRegistryError thread_registry_register(
    const ThreadConfig* thread,
    bool auto_cleanup
);

ThreadRegistryError thread_registry_update_state(
    const char* thread_label,
    ThreadState new_state
);

ThreadState thread_registry_get_state(
    const char* thread_label
);
```

### Message Queue Operations
```c
ThreadRegistryError init_queue(
    const char* thread_label
);

ThreadRegistryError push_message(
    const char* thread_label,
    const Message_T* message,
    uint32_t timeout_ms
);

ThreadRegistryError pop_message(
    const char* thread_label,
    Message_T* message,
    uint32_t timeout_ms
);
```

### Thread Synchronization
```c
PlatformWaitResult thread_registry_wait_for_thread(
    const char* thread_label,
    uint32_t timeout_ms
);

PlatformWaitResult thread_registry_wait_all(
    uint32_t timeout_ms
);

PlatformWaitResult thread_registry_wait_others(void);
```

## Error Handling
```c
typedef enum {
    THREAD_REG_SUCCESS = 0,
    THREAD_REG_NOT_INITIALIZED,
    THREAD_REG_INVALID_ARGS,
    THREAD_REG_DUPLICATE_THREAD,
    THREAD_REG_NOT_FOUND,
    THREAD_REG_CREATION_FAILED,
    THREAD_REG_LOCK_ERROR,
    THREAD_REG_QUEUE_FULL,
    THREAD_REG_QUEUE_EMPTY,
    THREAD_REG_INVALID_STATE_TRANSITION,
    THREAD_REG_UNAUTHORIZED
} ThreadRegistryError;
```

## Usage Examples

### Thread Registration
```c
ThreadConfig config = {
    .label = "worker_thread",
    .function = worker_function,
    .arg = worker_arg
};

ThreadRegistryError err = thread_registry_register(&config, true);
if (err != THREAD_REG_SUCCESS) {
    // Handle error
}
```

### Message Queue Usage
```c
// Initialize queue
ThreadRegistryError err = init_queue("worker_thread");
if (err == THREAD_REG_SUCCESS) {
    // Send message
    Message_T msg = { /* ... */ };
    err = push_message("worker_thread", &msg, 1000);
}
```

### Thread Synchronization
```c
// Wait for thread completion
PlatformWaitResult result = thread_registry_wait_for_thread(
    "worker_thread", 5000);
if (result == PLATFORM_WAIT_SUCCESS) {
    // Thread completed
}
```

## Thread Safety
- All public APIs are thread-safe
- Internal synchronization via platform mutex
- Message queues for inter-thread communication
- Atomic operations for state management

## Resource Management
- Automatic resource cleanup when enabled
- Thread completion events
- Message queue cleanup
- Proper mutex handling