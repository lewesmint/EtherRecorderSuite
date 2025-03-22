# Thread Registry System Design

## Core Components

1. **Thread Registry Structure**
   ```c
   typedef struct ThreadRegistry {
       ThreadRegistryEntry* head;    // Head of registry entries
       PlatformMutex_T mutex;       // Registry lock
       uint32_t count;              // Number of registered threads
   } ThreadRegistry;
   ```

2. **Thread Entry**
   ```c
   typedef struct ThreadRegistryEntry {
       const ThreadConfig* thread;           // Thread configuration
       ThreadState state;                    // Current thread state
       bool auto_cleanup;                    // Auto cleanup flag
       MessageQueue_T* queue;                // Message queue for this thread
       struct ThreadRegistryEntry* next;     // Next entry in list
       PlatformEvent_T completion_event;     // Event signaled on thread completion
   } ThreadRegistryEntry;
   ```

## Core Operations

1. **Registration**
   - `thread_registry_register()`: Adds new thread to registry
   - Validates thread config
   - Creates completion event
   - Adds to linked list
   - Thread starts in CREATED state

2. **State Management**
   - `thread_registry_update_state()`: Updates thread state
   - Valid states: CREATED → RUNNING → TERMINATED/FAILED
   - State changes protected by mutex

3. **Deregistration**
   - `thread_registry_deregister()`: Removes thread from registry
   - Cleans up resources if auto_cleanup set
   - Updates linked list

4. **Thread Monitoring**
   - Watchdog uses registry to check thread status
   - Detects mismatches between registry state and actual thread state
   - Updates registry when dead threads found

## Thread States
```c
typedef enum ThreadState {
    THREAD_STATE_CREATED,    // Thread created but not running
    THREAD_STATE_RUNNING,    // Thread is running
    THREAD_STATE_SUSPENDED,  // Thread temporarily suspended
    THREAD_STATE_STOPPING,   // Thread signaled to stop
    THREAD_STATE_TERMINATED, // Thread has terminated
    THREAD_STATE_FAILED,     // Thread encountered an error
    THREAD_STATE_UNKNOWN     // Thread state is unknown
} ThreadState;
```

## Thread Synchronization

1. **Wait Operations**
   - `thread_registry_wait_for_thread()`: Wait for specific thread
   - `thread_registry_wait_all()`: Wait for all threads
   - `thread_registry_wait_others()`: Wait for other threads

2. **Message Queue Operations**
   - Each thread gets a message queue
   - `push_message()`: Send message to thread
   - `pop_message()`: Receive message from queue

## Safety Features

1. **Mutex Protection**
   - All registry operations protected by mutex
   - Prevents concurrent modification

2. **Resource Cleanup**
   - Auto cleanup of thread resources
   - Completion events properly managed
   - Message queues cleaned up on deregistration

## Watchdog System

1. **Watchdog Thread**
   - Single dedicated thread for monitoring all registered threads
   - Runs periodic checks every 1 second
   - Uses thread registry to iterate through active threads
   - No heartbeat mechanism required - relies on OS-level thread status

2. **Core Responsibilities**
   - Detects threads that have died unexpectedly
   - Updates registry state when dead threads found (RUNNING → FAILED)
   - Logs error conditions for system monitoring
   - Does not attempt to restart threads (left to application logic)

3. **Implementation**
   ```c
   while (!shutdown_signalled()) {
       ThreadRegistryEntry* entry = thread_registry_find_thread(NULL);
       
       while (entry) {
           if (entry->state == THREAD_STATE_RUNNING) {
               // Check actual thread state via platform API
               PlatformThreadStatus status;
               if (platform_thread_get_status(entry->thread->thread_id, &status) 
                   != PLATFORM_ERROR_SUCCESS) {
                   logger_log(LOG_ERROR, "Thread '%s' status check failed", 
                            entry->thread->label);
               } 
               else if (status == PLATFORM_THREAD_DEAD) {
                   logger_log(LOG_ERROR, "Thread '%s' has died unexpectedly", 
                            entry->thread->label);
                   thread_registry_update_state(entry->thread->label, 
                                             THREAD_STATE_FAILED);
               }
           }
           entry = entry->next;
       }
       sleep_ms(1000);
   }
   ```

4. **Watchdog Self-Monitoring**
   - Main thread monitors watchdog health every 5 seconds via `check_watchdog()`
   - If watchdog dies, main thread will:
     - Deregister the dead watchdog
     - Create and start a new watchdog thread
     - Log the restart event

5. **Dependencies**
   - Requires platform API for thread status checking
   - Uses thread registry for thread iteration
   - Uses system logger for error reporting
