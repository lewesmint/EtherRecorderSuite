#include "platform_threads.h"

#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "platform_error.h"

PlatformErrorCode platform_thread_init(void) {
    return PLATFORM_ERROR_SUCCESS;  // No specific init needed for POSIX threads
}

void platform_thread_cleanup(void) {
    // No specific cleanup needed for POSIX threads
}

PlatformErrorCode platform_thread_create(
    PlatformThreadId* thread_id,
    const PlatformThreadAttributes* attributes,
    PlatformThreadFunction function,
    void* arg) 
{
    if (!thread_id) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    pthread_attr_t attr;
    
    if (pthread_attr_init(&attr) != 0) {
        return PLATFORM_ERROR_THREAD_CREATE;
    }

    if (attributes) {
        if (attributes->stack_size > 0) {
            if (pthread_attr_setstacksize(&attr, attributes->stack_size) != 0) {
                pthread_attr_destroy(&attr);
                return PLATFORM_ERROR_THREAD_CREATE;
            }
        }
        if (attributes->detached) {
            if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
                pthread_attr_destroy(&attr);
                return PLATFORM_ERROR_THREAD_CREATE;
            }
        }
    }

    pthread_t thread;
    int result = pthread_create(&thread, &attr, function, arg);
    pthread_attr_destroy(&attr);

    if (result != 0) {
        return PLATFORM_ERROR_THREAD_CREATE;
    }

    *thread_id = (PlatformThreadId)thread;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_thread_join(PlatformThreadHandle handle, void** result) 
{
    pthread_t thread = (pthread_t)(uintptr_t)handle;
    void* thread_result;
    
    int join_result = pthread_join(thread, &thread_result);
    if (join_result != 0) {
        return PLATFORM_ERROR_THREAD_JOIN;
    }

    if (result) {
        *result = thread_result;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_thread_detach(PlatformThreadHandle handle) 
{
    pthread_t thread = (pthread_t)(uintptr_t)handle;
    
    if (pthread_detach(thread) != 0) {
        return PLATFORM_ERROR_THREAD_DETACH;
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformThreadHandle platform_thread_get_handle(void) 
{
    return (PlatformThreadHandle)pthread_self();
}

PlatformThreadId platform_thread_get_id(void) 
{
    return (PlatformThreadId)pthread_self();
}

PlatformErrorCode platform_thread_set_priority(
    PlatformThreadHandle handle,
    PlatformThreadPriority priority) 
{
    pthread_t thread = (pthread_t)(uintptr_t)handle;
    struct sched_param param;
    int policy;

    if (pthread_getschedparam(thread, &policy, &param) != 0) {
        // Changed from PLATFORM_ERROR_THREAD_PRIORITY to PLATFORM_ERROR_UNKNOWN
        return PLATFORM_ERROR_UNKNOWN;
    }

    // Map platform priority to POSIX priority
    switch (priority) {
        case PLATFORM_THREAD_PRIORITY_LOWEST:
            param.sched_priority = sched_get_priority_min(policy);
            break;
        case PLATFORM_THREAD_PRIORITY_LOW:
            param.sched_priority = sched_get_priority_min(policy) + 
                (sched_get_priority_max(policy) - sched_get_priority_min(policy)) / 4;
            break;
        case PLATFORM_THREAD_PRIORITY_NORMAL:
            param.sched_priority = (sched_get_priority_min(policy) + 
                sched_get_priority_max(policy)) / 2;
            break;
        case PLATFORM_THREAD_PRIORITY_HIGH:
            param.sched_priority = sched_get_priority_max(policy) - 
                (sched_get_priority_max(policy) - sched_get_priority_min(policy)) / 4;
            break;
        case PLATFORM_THREAD_PRIORITY_HIGHEST:
            param.sched_priority = sched_get_priority_max(policy);
            break;
        case PLATFORM_THREAD_PRIORITY_REALTIME:
            policy = SCHED_RR;
            param.sched_priority = sched_get_priority_max(policy);
            break;
        default:
            return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (pthread_setschedparam(thread, policy, &param) != 0) {
        // Changed from PLATFORM_ERROR_THREAD_PRIORITY to PLATFORM_ERROR_UNKNOWN
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_thread_get_priority(
    PlatformThreadHandle handle,
    PlatformThreadPriority* priority) 
{
    pthread_t thread = (pthread_t)(uintptr_t)handle;
    struct sched_param param;
    int policy;

    if (!priority) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (pthread_getschedparam(thread, &policy, &param) != 0) {
        // Changed from PLATFORM_ERROR_THREAD_PRIORITY to PLATFORM_ERROR_UNKNOWN
        return PLATFORM_ERROR_UNKNOWN;
    }

    int min_prio = sched_get_priority_min(policy);
    int max_prio = sched_get_priority_max(policy);
    int prio_range = max_prio - min_prio;
    int curr_prio = param.sched_priority;

    // Map POSIX priority back to platform priority
    if (policy == SCHED_RR && curr_prio == max_prio) {
        *priority = PLATFORM_THREAD_PRIORITY_REALTIME;
    } else if (curr_prio >= max_prio) {
        *priority = PLATFORM_THREAD_PRIORITY_HIGHEST;
    } else if (curr_prio >= min_prio + (prio_range * 3) / 4) {
        *priority = PLATFORM_THREAD_PRIORITY_HIGH;
    } else if (curr_prio >= min_prio + (prio_range) / 2) {
        *priority = PLATFORM_THREAD_PRIORITY_NORMAL;
    } else if (curr_prio >= min_prio + (prio_range) / 4) {
        *priority = PLATFORM_THREAD_PRIORITY_LOW;
    } else {
        *priority = PLATFORM_THREAD_PRIORITY_LOWEST;
    }

    return PLATFORM_ERROR_SUCCESS;
}

void platform_thread_yield(void) 
{
    sched_yield();
}
