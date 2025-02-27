#include "platform_threads.h"

#ifdef _WIN32
typedef struct ThreadWrapper_T{
    ThreadFunc_T func;
    void *arg;
} ThreadWrapper_T;

DWORD WINAPI thread_start(LPVOID arg) {
    ThreadWrapper_T *wrapper = (ThreadWrapper_T *)arg;
    wrapper->func(wrapper->arg);
    free(wrapper);
    return 0;
}

int platform_thread_create(PlatformThread_T *thread, ThreadFunc_T func, void *arg) {
	// be sure to release this memory
    ThreadWrapper_T *wrapper = (ThreadWrapper_T *)malloc(sizeof(ThreadWrapper_T));
    if (!wrapper) return -1;
    wrapper->func = func;
    wrapper->arg = arg;
    *thread = CreateThread(NULL, 0, thread_start, wrapper, 0, NULL);   
    return (*thread == NULL) ? -1 : 0;
}

int platform_thread_join(PlatformThread_T thread, void **retval) {
    (void)retval; // Unused
    return (WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0) ? 0 : -1;
}

#else //!_WIN32

int platform_thread_create(PlatformThread_T *thread, ThreadFunc_T func, void *arg) {
    return pthread_create(thread, NULL, func, arg);
}

int platform_thread_join(PlatformThread_T thread, void **retval) {
    return pthread_join(thread, retval);
}

#endif // _WIN32
