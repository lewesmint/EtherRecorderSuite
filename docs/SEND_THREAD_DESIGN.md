# Send Thread Implementation Guide

## Prerequisites
1. Required Headers
   ```c
   #include "thread_registry.h"
   #include "message_queue.h"
   #include "platform_socket.h"
   #include "logger.h"
   #include "comm_context.h"
   ```

## Core Components

### 1. Thread Configuration
```c
ThreadConfig* create_send_thread_config(
    const char* label, 
    CommContext* context,
    PlatformThreadHandle master_handle  // Master thread handle for group ID
) {
    ThreadConfig* config = malloc(sizeof(ThreadConfig));
    if (!config) return NULL;

    config->label = strdup(label);
    config->data = context;
    config->group_id = master_handle;  // Set group ID to master's handle
    
    return config;
}
```

### 2. Message Processing
```c
ThreadResult send_message_processor(ThreadConfig* config, Message_T* message) {
    CommContext* context = (CommContext*)config->data;
    
    // Check if this is a relay message
    bool is_relay = (message->header.type == MSG_TYPE_RELAY);
    
    if (!is_socket_writable(context->socket, SOCKET_WRITE_TIMEOUT_MS)) {
        return THREAD_ERROR;
    }

    // For relay messages, additional logging may be needed
    if (is_relay && context->is_relay_enabled) {
        // Get foreign queue using thread label
        MessageQueue_T* foreign_queue = get_queue_by_label(context->foreign_queue_label);
        if (foreign_queue) {
            logger_log(LOG_DEBUG, "Processing relay message to thread %s", 
                      context->foreign_queue_label);
        }
    }

    PlatformErrorCode err = platform_socket_send(
        context->socket,
        message->content,
        message->header.content_size,
        0
    );

    return (err == PLATFORM_ERROR_SUCCESS) ? THREAD_SUCCESS : THREAD_ERROR;
}
```

### 3. Integration Example
```c
bool start_send_thread(CommContext* context, PlatformThreadHandle master_handle) {
    ThreadConfig* config = create_send_thread_config("SEND", context, master_handle);
    if (!config) return false;
    
    ThreadRegistryError result = thread_registry_create_thread(config);
    if (result != THREAD_SUCCESS) {
        free((void*)config->label);
        free(config);
        return false;
    }
    
    return true;
}
```

## Thread Label Usage
- Each thread is identified by a unique label
- Relay configuration uses these labels to identify target threads
- Actual queue lookup happens at runtime via thread registry
- This provides better decoupling between threads

## Testing Requirements

1. Unit Tests
   - Message processing
   - Queue operations
   - Error handling
   - Stats collection

2. Integration Tests
   - With real sockets
   - Multiple threads
   - Relay scenarios
   - Error scenarios

3. Performance Tests
   - Message throughput
   - Memory usage
   - CPU utilization
   - Latency measurements

## Common Issues and Solutions

1. Queue Full
   - Implement backpressure
   - Monitor queue size
   - Alert when near capacity

2. Socket Errors
   - Implement reconnection logic
   - Use exponential backoff
   - Log error patterns

3. Memory Leaks
   - Clean up message resources
   - Monitor memory usage
   - Regular leak checking

4. Performance
   - Tune batch sizes
   - Adjust intervals
   - Profile hot paths

Would you like me to expand on any of these sections or add more details?
