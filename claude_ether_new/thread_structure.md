# Thread Structure and Data Flow

## Thread Hierarchy

### 1. Base Thread Structure
All threads use the common `ThreadConfig` structure:
```c
typedef struct ThreadConfig {
    const char* label;                   // Thread identifier
    ThreadFunc_T func;                   // Main thread function
    PlatformThreadHandle thread_id;      // Platform-agnostic thread identifier
    void *data;                          // Thread-specific context data
    PreCreateFunc_T pre_create_func;     // Pre-creation hook
    PostCreateFunc_T post_create_func;   // Post-creation hook
    InitFunc_T init_func;                // Initialization hook
    ExitFunc_T exit_func;                // Cleanup hook
    bool suppressed;                     // Thread suppression flag
    
    // Queue processing related fields
    MessageProcessor_T msg_processor;     // Message processing callback
    uint32_t queue_process_interval_ms;  // How often to check queue (0 = every loop)
    uint32_t max_process_time_ms;        // Max time to spend processing queue (0 = no limit)
    uint32_t msg_batch_size;             // Max messages to process per batch (0 = no limit)
} ThreadConfig;
```

### 2. Main Communication Threads

#### Client Thread
- Uses `ThreadConfig` where `data` points to:
```c
typedef struct {
    int port;                   // Port number
    bool send_test_data;        // Send test data flag
    int send_interval_ms;       // Send interval
    int data_size;             // Data size
} CommsThreadArgs_T;
```

#### Server Thread
- Uses `ThreadConfig` where `data` points to:
```c
typedef struct {
    char server_hostname[100];  // Server hostname or IP address
    CommsThreadArgs_T comm_args;// Common communication arguments
} ServerCommsThreadArgs_T;
```

## Example Configurations

```c
ThreadConfig logger_thread = {
    .label = "LOGGER",
    .func = logger_thread_function,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_stub,
    .exit_func = exit_stub,
    .suppressed = false,
    .msg_processor = logger_message_processor,
    .queue_process_interval_ms = 0,
    .max_process_time_ms = 100,
    .msg_batch_size = 10
};
```

### 3. Send/Receive Threads
When a connection is established, both client and server spawn send/receive threads.

#### Current Structure
```c
typedef struct {
    CommContext* context;       // Shared communication context
    void* thread_info;         // Additional thread info
    void* queue;              // Message queue
    uint32_t thread_id;       // Thread ID
} CommsArgs_T;
```

#### Client Thread Arguments
```c
typedef struct {
    int port;                // Port number
    bool send_test_data;     // Send test data flag
    int send_interval_ms;    // Send interval
    int data_size;          // Data size
} CommsThreadArgs_T;

typedef struct {
    char server_hostname[100];  // Server hostname or IP address
    CommsThreadArgs_T comm_args;// Common communication arguments
} ClientCommsThreadArgs_T;
```

### 4. Shared Communication Context
```c
typedef struct {
    SOCKET* socket;                      // Socket handle
    volatile long connection_closed;      // Connection status flag
    uint32_t group_id;                   // Group identifier
    struct sockaddr_in client_addr;      // Client address
    bool is_relay_enabled;               // Relay mode enabled flag
    bool is_tcp;                         // Protocol type (TCP/UDP)
    void* queue;                         // Message queue
    uint32_t thread_id;                  // Thread identifier
} CommContext;
```

## Thread Organization

1. Main thread creates either client or server thread
   - Initializes with appropriate configuration structure
   - Server thread becomes group master for its worker threads
   - Client thread becomes group master for its communication threads

2. When connection established:
   - Client/Server thread creates send/receive threads as group members
   - Each thread gets `CommsArgs_T` with:
     - Pointer to shared `CommContext`
     - Appropriate message queue for communication

3. Send/Receive threads:
   - Use shared `CommContext` for socket operations
   - Use message queue for inter-thread communication
   - Belong to their creator's thread group

## Proposed Simplifications

1. Remove redundant fields from `CommsArgs_T`:
   - `thread_id` (already in `AppThread_T`)
   - `thread_info` (not necessary for send/receive operation)

2. Consider consolidating queue management:
   - Queue could be part of `CommContext`
   - Or maintain separate send/receive queues in context

These changes would simplify the structure while maintaining all necessary functionality.
