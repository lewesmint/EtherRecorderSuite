
/**
 * @file posix_sync.c
 * @brief POSIX implementation of platform synchronization primitives (events, waits, signals)
 */

#include "platform_sync.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#include "platform_threads.h"
#include "platform_time.h"    // For sleep_ms function
#include "platform_error.h"


struct platform_event {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool signaled;
    bool manual_reset;
};

struct platform_thread_wait_context {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool completed;
    pthread_t thread;
};

static void* thread_wait_wrapper(void* arg) {
    struct platform_thread_wait_context* context = 
        (struct platform_thread_wait_context*)arg;
    
    // Wait for the actual thread
    pthread_join(context->thread, NULL);
    
    // Signal completion
    pthread_mutex_lock(&context->mutex);
    context->completed = true;
    pthread_cond_signal(&context->cond);
    pthread_mutex_unlock(&context->mutex);
    
    return NULL;
}

PlatformWaitResult platform_thread_wait_single(PlatformThreadId thread_id, uint32_t timeout_ms) {
    struct platform_thread_wait_context context;
    pthread_mutex_init(&context.mutex, NULL);
    pthread_cond_init(&context.cond, NULL);
    context.completed = false;
    context.thread = (pthread_t)thread_id;  // Direct cast since they're the same type

    // Create wrapper thread to handle the join
    pthread_t wrapper_thread;
    if (pthread_create(&wrapper_thread, NULL, thread_wait_wrapper, &context) != 0) {
        pthread_mutex_destroy(&context.mutex);
        pthread_cond_destroy(&context.cond);
        return PLATFORM_WAIT_ERROR;
    }

    // Wait with timeout
    struct timespec ts;
    if (timeout_ms != PLATFORM_WAIT_INFINITE) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
    }

    pthread_mutex_lock(&context.mutex);
    int result;
    if (timeout_ms == PLATFORM_WAIT_INFINITE) {
        result = pthread_cond_wait(&context.cond, &context.mutex);
    } else {
        result = pthread_cond_timedwait(&context.cond, &context.mutex, &ts);
    }
    bool completed = context.completed;
    pthread_mutex_unlock(&context.mutex);

    pthread_detach(wrapper_thread);
    pthread_mutex_destroy(&context.mutex);
    pthread_cond_destroy(&context.cond);

    if (completed) {
        return PLATFORM_WAIT_SUCCESS;
    } else if (result == ETIMEDOUT) {
        return PLATFORM_WAIT_TIMEOUT;
    }
    return PLATFORM_WAIT_ERROR;
}

PlatformWaitResult platform_wait_multiple(PlatformThreadId *thread_list, 
                                        uint32_t count, 
                                        bool wait_all, 
                                        uint32_t timeout_ms) {
    if (count == 0) {
        return PLATFORM_WAIT_SUCCESS;
    }
    
    uint32_t time_left = timeout_ms;
    struct timespec start, end;
    
    if (timeout_ms != PLATFORM_WAIT_INFINITE) {
        clock_gettime(CLOCK_MONOTONIC, &start);
    }
    
    bool* thread_done = (bool*)calloc(count, sizeof(bool));
    if (!thread_done) {
        return PLATFORM_WAIT_ERROR;
    }
    
    size_t done_count = 0;
    
    while (done_count < (wait_all ? count : 1)) {
        if (timeout_ms != PLATFORM_WAIT_INFINITE && time_left == 0) {
            free(thread_done);
            return PLATFORM_WAIT_TIMEOUT;
        }
        
        for (size_t i = 0; i < count; i++) {
            if (!thread_done[i]) {
                PlatformWaitResult result = platform_thread_wait_single(thread_list[i], 
                                                                      wait_all ? 0 : time_left);
                
                if (result == PLATFORM_WAIT_SUCCESS) {
                    thread_done[i] = true;
                    done_count++;
                    
                    if (!wait_all) {
                        free(thread_done);
                        return PLATFORM_WAIT_SUCCESS;
                    }
                } else if (!wait_all && result == PLATFORM_WAIT_TIMEOUT) {
                    free(thread_done);
                    return PLATFORM_WAIT_TIMEOUT;
                }
            }
        }
        
        // If we're waiting for all threads and not all are done, sleep briefly
        if (wait_all && done_count < count) {
            sleep_ms(10);  // Short sleep to avoid CPU spinning
            
            if (timeout_ms != PLATFORM_WAIT_INFINITE) {
                clock_gettime(CLOCK_MONOTONIC, &end);
                uint32_t elapsed = (end.tv_sec - start.tv_sec) * 1000 + 
                                 (end.tv_nsec - start.tv_nsec) / 1000000;
                
                if (elapsed >= time_left) {
                    free(thread_done);
                    return PLATFORM_WAIT_TIMEOUT;
                }
                
                time_left = timeout_ms - elapsed;
            }
        }
    }
    
    free(thread_done);
    return PLATFORM_WAIT_SUCCESS;
}

// Array to store handlers for different signal types
static PlatformSignalHandler g_handlers[2] = {NULL};

// Convert platform signal type to POSIX signal number
static int get_posix_signal(PlatformSignalType type) {
    switch (type) {
        case PLATFORM_SIGNAL_INT:  return SIGINT;
        case PLATFORM_SIGNAL_TERM: return SIGTERM;
        default: return -1;
    }
}

// Generic signal handler that dispatches to registered handlers
static void signal_handler(int sig) {
    PlatformSignalType type;
    switch (sig) {
        case SIGINT:  
            type = PLATFORM_SIGNAL_INT;  
            const char* msg = "SIGINT received\n";
            write(STDERR_FILENO, msg, strlen(msg));
            break;
        case SIGTERM: 
            type = PLATFORM_SIGNAL_TERM; 
            const char* msg_term = "SIGTERM received\n";
            write(STDERR_FILENO, msg_term, strlen(msg_term));
            break;
        default: return;
    }
    
    if (g_handlers[type]) {
        g_handlers[type]();
    }
}

bool platform_signal_register_handler(PlatformSignalType signal_type, 
                                    PlatformSignalHandler handler) {
    if (!handler || signal_type < 0 || signal_type >= 2) {
        return false;
    }

    int posix_signal = get_posix_signal(signal_type);
    if (posix_signal == -1) {
        return false;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(posix_signal, &sa, NULL) != 0) {
        return false;
    }

    g_handlers[signal_type] = handler;
    return true;
}

bool platform_signal_unregister_handler(PlatformSignalType signal_type) {
    if (signal_type < 0 || signal_type >= 2) {
        return false;
    }

    int posix_signal = get_posix_signal(signal_type);
    if (posix_signal == -1) {
        return false;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(posix_signal, &sa, NULL) != 0) {
        return false;
    }

    g_handlers[signal_type] = NULL;
    return true;
}

PlatformErrorCode platform_event_create(PlatformEvent_T* event, bool manual_reset, bool initial_state) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct platform_event* evt = malloc(sizeof(struct platform_event));
    if (!evt) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    
    if (pthread_mutex_init(&evt->mutex, &mutex_attr) != 0) {
        pthread_mutexattr_destroy(&mutex_attr);
        free(evt);
        return PLATFORM_ERROR_SYSTEM;
    }
    
    pthread_mutexattr_destroy(&mutex_attr);
    
    if (pthread_cond_init(&evt->cond, NULL) != 0) {
        pthread_mutex_destroy(&evt->mutex);
        free(evt);
        return PLATFORM_ERROR_SYSTEM;
    }
    
    evt->signaled = initial_state;
    evt->manual_reset = manual_reset;
    *event = evt;
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_event_destroy(PlatformEvent_T event) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->mutex);
    free(event);
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_event_set(PlatformEvent_T event) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    
    if (event->manual_reset) {
        pthread_cond_broadcast(&event->cond);
    } else {
        pthread_cond_signal(&event->cond);
    }
    
    pthread_mutex_unlock(&event->mutex);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_event_reset(PlatformEvent_T event) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&event->mutex);
    event->signaled = false;
    pthread_mutex_unlock(&event->mutex);
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_event_wait(PlatformEvent_T event, uint32_t timeout_ms) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&event->mutex);
    
    if (!event->signaled) {
        if (timeout_ms == PLATFORM_WAIT_INFINITE) {
            while (!event->signaled) {
                pthread_cond_wait(&event->cond, &event->mutex);
            }
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout_ms / 1000;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }

            int ret;
            while (!event->signaled) {
                ret = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
                if (ret == ETIMEDOUT) {
                    pthread_mutex_unlock(&event->mutex);
                    return PLATFORM_ERROR_TIMEOUT;
                }
                if (ret != 0) {
                    pthread_mutex_unlock(&event->mutex);
                    return PLATFORM_ERROR_SYSTEM;
                }
            }
        }
    }

    if (!event->manual_reset) {
        event->signaled = false;
    }
    
    pthread_mutex_unlock(&event->mutex);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformWaitResult platform_wait_single(PlatformThreadId thread_id, 
                                      uint32_t timeout_ms) {
    if (!thread_id) {
        return PLATFORM_WAIT_ERROR;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)thread_id;
    int result = 0;
    
    if (timeout_ms == PLATFORM_WAIT_INFINITE) {
        result = pthread_mutex_lock(mutex);
        if (result == 0) {
            pthread_mutex_unlock(mutex);
            return PLATFORM_WAIT_SUCCESS;
        }
    } else {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            return PLATFORM_WAIT_ERROR;
        }
        
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        
        // Try to acquire the lock in a loop with timeout
        struct timespec sleep_time = {0, 1000000}; // 1ms sleep
        struct timespec remaining;
        
        while (1) {
            result = pthread_mutex_trylock(mutex);
            if (result == 0) {
                break; // Lock acquired
            }
            if (result != EBUSY) {
                return PLATFORM_WAIT_ERROR;
            }
            
            // Check if we've exceeded timeout
            struct timespec now;
            if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
                return PLATFORM_WAIT_ERROR;
            }
            
            if ((now.tv_sec > ts.tv_sec) || 
                (now.tv_sec == ts.tv_sec && now.tv_nsec >= ts.tv_nsec)) {
                return PLATFORM_WAIT_TIMEOUT;
            }
            
            // Sleep for a short duration
            nanosleep(&sleep_time, &remaining);
        }
    }
    
    return PLATFORM_WAIT_ERROR;
}
