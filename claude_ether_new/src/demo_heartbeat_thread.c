#include "demo_heartbeat_thread.h"
#include "logger.h"
#include "platform_threads.h"
#include "shutdown_handler.h"
#include "message_types.h"
#include "thread_registry.h"
#include "thread_status_errors.h"

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
            return (void*)result;
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
        demo_heartbeat_thread = ThreadConfigemplate;  // Copy all default values
        demo_heartbeat_thread.label = "DEMO_HEARTBEAT";
        demo_heartbeat_thread.func = demo_heartbeat_function;
        demo_heartbeat_thread.msg_processor = process_demo_message;
        initialized = true;
    }
    return &demo_heartbeat_thread;
}
