# Thread Structure and Data Flow

## Thread Hierarchy

### 1. Base Thread Structure
All threads use the common `AppThread_T` structure:
```c
typedef struct AppThread_T {
    const char* label;                   // Thread identifier
    ThreadFunc_T func;                   // Main thread function
    ThreadHandle_T thread_id;            // Platform-agnostic thread ID
    void *data;                         // Thread-specific context data
    PreCreateFunc_T pre_create_func;     // Pre-creation hook
    PostCreateFunc_T post_create_func;   // Post-creation hook
    InitFunc_T init_func;                // Initialization hook
    ExitFunc_T exit_func;                // Cleanup hook
    bool suppressed;                     // Thread suppression flag
} AppThread_T;
```

### 2. Main Communication Threads

#### Client Thread
- Uses `AppThread_T` where `data` points to:
```c
typedef struct {
    int port;                   // Port number
    bool send_test_data;        // Send test data flag
    int send_interval_ms;       // Send interval
    int data_size;             // Data size
} CommsThreadArgs_T;
```

#### Server Thread
- Uses `AppThread_T` where `data` points to:
```c
typedef struct {
    char server_hostname[100];  // Server hostname or IP address
    CommsThreadArgs_T comm_args;// Common communication arguments
} ServerCommsThreadArgs_T;
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

## Data Flow

1. Main thread creates either client or server thread
   - Initializes with appropriate configuration structure

2. When connection established:
   - Client/Server thread creates send/receive threads
   - Each thread gets `CommsArgs_T` with:
     - Pointer to shared `CommContext`
     - Appropriate message queue for communication

3. Send/Receive threads:
   - Use shared `CommContext` for socket operations
   - Use message queue for inter-thread communication

## Proposed Simplifications

1. Remove redundant fields from `CommsArgs_T`:
   - `thread_id` (already in `AppThread_T`)
   - `thread_info` (not necessary for send/receive operation)

2. Consider consolidating queue management:
   - Queue could be part of `CommContext`
   - Or maintain separate send/receive queues in context

These changes would simplify the structure while maintaining all necessary functionality.
