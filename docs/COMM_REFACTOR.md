# Communication Thread Management Refactoring

## Core Structures

### Structure Definitions

```c
// Base thread structure used by all threads
typedef struct ThreadConfig {
    const char* label;                   ///< Thread identifier
    ThreadFunc_T func;                   ///< Main thread function
    PlatformThreadHandle thread_id;      ///< Platform-agnostic thread identifier
    void *data;                          ///< Thread-specific context data
    PreCreateFunc_T pre_create_func;     ///< Pre-creation hook
    PostCreateFunc_T post_create_func;   ///< Post-creation hook
    InitFunc_T init_func;                ///< Initialization hook
    ExitFunc_T exit_func;                ///< Cleanup hook
    bool suppressed;                     ///< Thread suppression flag
    
    // Queue processing related fields
    MessageProcessor_T msg_processor;     ///< Message processing callback
    uint32_t queue_process_interval_ms;  ///< How often to check queue (0 = every loop)
    uint32_t max_process_time_ms;        ///< Max time to spend processing queue (0 = no limit)
    uint32_t msg_batch_size;             ///< Max messages to process per batch (0 = no limit)
} ThreadConfig;

// Shared context for send/receive threads
typedef struct CommContext {
    SOCKET* socket;                      ///< Socket handle
    volatile long connection_closed;      ///< Connection status flag
    uint32_t group_id;                   ///< Group identifier
    struct sockaddr_in client_addr;      ///< Client address
    bool is_relay_enabled;               ///< Relay mode enabled flag
    bool is_tcp;                         ///< Protocol type (TCP/UDP)
    void* queue;                         ///< Message queue
    uint32_t thread_id;                  ///< Thread identifier
    const char* owner_label;             ///< Owner thread label
} CommContext;

// Send/Receive thread arguments
typedef struct CommsArgs_T {
    CommContext* context;                ///< Shared communication context
    void* thread_info;                  ///< Additional thread info
    void* queue;                        ///< Message queue
    uint32_t thread_id;                 ///< Thread identifier
    const char* owner_label;            ///< Owner thread label
} CommsArgs_T;

// Client/Server configuration parameters
typedef struct CommsThreadArgs_T {
    int port;                           ///< Port number
    bool send_test_data;                ///< Send test data flag
    int send_interval_ms;               ///< Send interval
    int data_size;                      ///< Data size
    const char* owner_label;            ///< Owner thread label
} CommsThreadArgs_T;

typedef struct ClientCommsThreadArgs_T {
    char server_hostname[100];           ///< Server hostname or IP address
    CommsThreadArgs_T comm_args;         ///< Common communication arguments
    const char* owner_label;             ///< Owner thread label
} ClientCommsThreadArgs_T;
```

## Goals
- Simplify thread coordination architecture
- Improve shared resource management
- Remove ThreadGroup abstraction in favor of lighter group_id based coordination
- Clarify cleanup responsibilities

## New Architecture

### Core Component: CommContext
Shared context containing runtime state:
- [x] Socket handle
- [x] Connection status flag
- [x] Master thread handle (for group identification)  # Updated from "Group ID"
- [x] Client address
- [x] Protocol type (TCP/UDP)
- [x] Message queue
- [x] Thread ID
- [x] Relay enabled flag

### Core Components
- [x] Shared CommContext pointer
- [x] Additional thread info
- [x] Message queue
- [x] Thread ID

### Core Component: CommsThreadArgs_T
Configuration/setup only parameters:
- [x] Port number
- [x] Send test data flag
- [x] Send interval
- [x] Data size

### Core Component: ClientCommsThreadArgs_T
Client-specific configuration:
- [x] Server hostname
- [x] Common CommsThreadArgs_T parameters

### Workflow
1. [ ] Client/Server Manager:
   - [x] Creates CommContext with socket and group_id
   - [x] Initializes CommsArgs_T with context
   - [ ] Spawns send/receive threads with shared context

2. [ ] Thread Registration:
   - [ ] Each thread registers with registry using group_id
   - [ ] Registry tracks thread count per group
   - [ ] Registry maintains thread state and relationships

3. [ ] Resource Cleanup:
   - [ ] Registry tracks last thread in group
   - [ ] Last thread responsible for CommContext cleanup
   - [ ] Automatic cleanup trigger when group empty

## Implementation Tasks

### Phase 1: Structure Setup
- [x] Define CommContext structure for shared runtime state
- [x] Define CommsArgs_T for thread arguments
- [x] Define CommsThreadArgs_T for configuration parameters
- [x] Define ClientCommsThreadArgs_T for client-specific config
- [x] Update existing code to use new structure separation
- [x] Add CommContext management functions

### Phase 2: Thread Management
- [x] Update thread creation to use correct argument structures:
  - [x] Client threads using CommsArgs_T
  - [x] Send/Receive threads using CommsArgs_T with CommContext
  - [x] Proper initialization of all structure fields
  - [x] Group membership via master thread handle  # Added this line

### Phase 3: Update Managers
- [ ] Modify Client Manager:
  - [ ] Update connection initialization with CommsArgs_T
  - [ ] Proper handling of ClientCommsThreadArgs_T configuration
  - [ ] Set as master thread for its worker threads  # Updated this line

- [ ] Modify Server Manager:
  - [ ] Update accept loop with CommsArgs_T
  - [ ] Revise client handling with proper context sharing
  - [ ] Set as master thread for connection handlers  # Updated this line

### Phase 4: Registry Updates
- [ ] Add master thread handle field for group identification  # Updated from "group_id based tracking"
- [ ] Implement group member detection
- [ ] Add automatic cleanup triggers

## Testing
- [ ] Unit tests for CommContext
- [ ] Integration tests for thread coordination
- [ ] Stress tests for cleanup handling
- [ ] Connection management tests

## Documentation
- [ ] Update API documentation
- [ ] Add migration guide
- [ ] Update thread coordination docs
