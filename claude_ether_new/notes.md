# Send Thread Analysis

This is the current send thread code, there's a populat_buffer functino in it.

## Current Implementation Code
```c
void* comm_send_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    CommContext* context = (CommContext*)thread_config->data;
    if (!context) {
        return NULL;
    }

    char buffer[2048];
    bool send_error = false;

    logger_log(LOG_INFO, "Send thread started");

    while (!comm_context_is_closed(context) && !shutdown_signalled() && !send_error) {
        size_t bytes_sent = 0;
        int32_t bytes_to_send = populate_buffer(buffer, sizeof(buffer));
        
        // Validate bytes_to_send
        if (bytes_to_send < 0 || (size_t)bytes_to_send > sizeof(buffer)) {
            logger_log(LOG_ERROR, "Invalid bytes_to_send: %d, max: %zu", bytes_to_send, sizeof(buffer));
            break;
        }

        // Send all data
        size_t total_sent = 0;
        while (total_sent < (size_t)bytes_to_send && !send_error) {
            PlatformErrorCode result = handle_send(context, 
                                                 buffer + total_sent, 
                                                 (size_t)bytes_to_send - total_sent, 
                                                 &bytes_sent);
            if (result != PLATFORM_ERROR_SUCCESS) {
                if (result == PLATFORM_ERROR_TIMEOUT) {
                    continue;  // Try again on timeout
                }
                send_error = true;
                break;
            }
            total_sent += bytes_sent;
        }

        if (!send_error) {
            sleep_ms(10);
        }
    }

    if (send_error) {
        logger_log(LOG_ERROR, "Send error occurred");
    }
    printf("Send thread Out of here\n");
    fflush(stdout);
    return NULL;
}
```

## Analysis of Current Implementation
- Main loop that tries to send data continuously
- Uses `populate_buffer()` which is currently stubbed out (returns 0)
- Has error handling for send operations
- Sleeps 10ms between iterations if no error
- No queue servicing currently implemented

## Issues
1. Not servicing the message queue at all
2. `populate_buffer()` is not doing anything useful
3. The thread is essentially just sleeping in a loop

## Required Changes
1. Need to service the thread's queue using `service_thread_queue()`
2. Already have `comms_thread_message_processor()` implemented for handling messages
3. Need to integrate queue servicing into the main loop

## Design Considerations
- Should we keep the current send loop structure?
- How to balance queue servicing with other potential send operations?
- Error handling strategy for queue vs direct send operations


This is how the demo_hearbeat_thread.c looks does its thing:

static ThreadResult process_demo_message(ThreadConfig* thread, const Message_T* message) {
    (void)thread; // Unused parameter

    if (message->header.type == MSG_TYPE_TEST) {
        if (message->header.content_size > 0 && message->header.content_size <= sizeof(message->content)) {
            logger_log(LOG_INFO, "Demo thread received message: %.*s", 
        
                (int)message->header.content_size,
                      (const char*)message->content);
        } else {
            logger_log(LOG_ERROR, "Received message with invalid size: %u", 
                      message->header.content_size);
        }
    }
    return THREAD_SUCCESS;
}

static void* demo_heartbeat_function(void* arg) {
    ThreadConfig* thread_info = (ThreadConfig*)arg;
    
    logger_log(LOG_INFO, "Demo heartbeat thread started");
    
    while (!shutdown_signalled()) {
        // Process any pending messages
        ThreadResult result = service_thread_queue(thread_info);
        if (result != THREAD_SUCCESS) {
            return (void*)(uintptr_t)(result);
        }
        
        // Do other periodic work
        logger_log(LOG_INFO, "Demo heartbeat");
        sleep_ms(3000);
    }
    
    logger_log(LOG_INFO, "Demo heartbeat thread shutting down");
    return (void*)THREAD_STATUS_SUCCESS;
}

// Define the demo thread structure
static ThreadConfig demo_heartbeat_thread;

ThreadConfig* get_demo_heartbeat_thread(void) {
    // Initialize from template if not already done
    static bool initialized = false;
    if (!initialized) {
        demo_heartbeat_thread = ThreadConfigTemplate;  // Copy all default values
        demo_heartbeat_thread.label = "DEMO_HEARTBEAT";
        demo_heartbeat_thread.func = demo_heartbeat_function;
        demo_heartbeat_thread.msg_processor = process_demo_message;
        initialized = true;
    }
    return &demo_heartbeat_thread;
}

So the the thread's msg_processor is set to process_demo_message.

This gets called in the main loop of the thread because it calls service_thread_queue()

Which looks like this:

ThreadResult service_thread_queue(ThreadConfig* thread) {
    if (!thread || !thread->msg_processor) {
        return THREAD_SUCCESS; // No processor = nothing to do
    }

    uint32_t start_time = get_time_ms();
    uint32_t messages_processed = 0;
    ThreadResult result = THREAD_SUCCESS;
    Message_T message;

    while (true) {
        // Check time limit
        if (thread->max_process_time_ms > 0) {
            uint32_t elapsed = get_time_ms() - start_time;
            if (elapsed >= thread->max_process_time_ms) {
                break;
            }
        }

        // Check message batch limit
        if (thread->msg_batch_size > 0 && messages_processed >= thread->msg_batch_size) {
            break;
        }

        // Try to get a message (non-blocking)
        ThreadRegistryError queue_result = pop_message(thread->label, &message, 0);
        
        if (queue_result == THREAD_REG_QUEUE_EMPTY) {
            break;
        }
        else if (queue_result == THREAD_REG_SUCCESS) {
            result = thread->msg_processor(thread, &message);
            messages_processed++;
            
            if (result != THREAD_SUCCESS) {
                logger_log(LOG_ERROR, "Message processing failed in thread '%s': %d", 
                          thread->label, result);
                break;
            }
        }
        else {
            logger_log(LOG_ERROR, "Queue access error in thread '%s': %s",
                      thread->label,
                      app_error_get_message(THREAD_REGISTRY_DOMAIN, queue_result));
            result = THREAD_ERROR_QUEUE_ERROR; // Use ThreadResult enum value instead of ThreadStatus
            break;
        }
    }

    return result;
}

So we will need to set the msg_processor to comms_thread_message_processor() and have to think what that should look like:

We also need to review the current version of the send_thread main function above, with the stubbed out populate_buffer() function.

Should we fill in that function or should we have someting else:

It might simply be better to use the buffer fro mthe queues message itself.
The next question. ao the service_thread_queue uses values established in the thread_config, and we can change those. Our scenario is that th we have a vast amount of date throwing through the app to be relayed. So we must be efficient, do we simply process one message at a time? I think if we are directly using the buffer fro mthe messages then we do.

So I tink we shojuld probsbly just be popping the queue and sending the message. without any fuss, and I wonder whther we shold bother with the comm_context servive_message_processor settings function.


# Proposed New Send Thread Design

## Key Design Decisions
1. Remove `populate_buffer()` - using message buffers directly
2. Simplify to basic queue processing
3. Direct send from message buffer to socket
4. Ensure complete message transmission
5. Single message processing for reliability

## Proposed Send Thread Implementation
```c
void* comm_send_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    CommContext* context = (CommContext*)thread_config->data;
    if (!context) {
        return NULL;
    }

    logger_log(LOG_INFO, "Send thread started");

    Message_T message;
    while (!comm_context_is_closed(context) && !shutdown_signalled()) {
        // Simple non-blocking message pop
        ThreadRegistryError queue_result = pop_message(thread_config->label, &message, 0);
        
        if (queue_result == THREAD_REG_QUEUE_EMPTY) {
            sleep_ms(10);
            continue;
        }
        
        if (queue_result != THREAD_REG_SUCCESS) {
            logger_log(LOG_ERROR, "Queue error in send thread");
            break;
        }

        // Send complete message with retry on partial sends
        size_t total_sent = 0;
        while (total_sent < message.header.content_size) {
            size_t bytes_sent = 0;
            PlatformErrorCode result = platform_socket_send(
                context->socket,
                message.content + total_sent,
                message.header.content_size - total_sent,
                &bytes_sent
            );

            if (result == PLATFORM_ERROR_SUCCESS) {
                total_sent += bytes_sent;
            }
            else if (result == PLATFORM_ERROR_TIMEOUT) {
                continue;  // Retry on timeout
            }
            else {
                logger_log(LOG_ERROR, "Send error occurred");
                comm_context_close(context);
                return NULL;
            }
        }
    }

    logger_log(LOG_INFO, "Send thread shutting down");
    return NULL;
}
```

## Benefits
1. Maintains simplicity while ensuring complete transmission
2. Handles partial sends properly
3. No unnecessary buffer copying
4. Proper error handling and connection cleanup
5. Maintains existing timeout and retry behavior

## Key Improvements Over Previous Version
1. Removed unnecessary `populate_buffer()`
2. Direct message buffer transmission
3. Maintained robust complete-send logic
4. Cleaner error handling
5. Still handles partial sends like original version

## Questions
1. Should we add a maximum retry count for timeouts?
2. Do we need to log partial send progress?
3. Should we add any performance metrics?
