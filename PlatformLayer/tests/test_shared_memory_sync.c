/**
 * @file test_shared_memory_sync.c
 * @brief Test program for shared memory synchronization
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "shared_memory_sync.h"
#include "win_shared_memory.h"
#include "logger.h"
#include "platform_thread.h"
#include "platform_time.h"
#include "shutdown_manager.h"
#include "thread_registry.h"

#define TEST_SHM_NAME "TestSharedMemory"
#define TEST_SHM_SIZE 1024

// Global handles
static WinSharedMemoryHandle g_reader_handle = NULL;
static WinSharedMemoryHandle g_writer_handle = NULL;

void print_error(const char* operation, PlatformErrorCode error) {
    printf("Error during %s: %d\n", operation, error);

    // Print Windows error information
    DWORD win_error = GetLastError();
    if (win_error != 0) {
        char error_msg[256];
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            win_error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            error_msg,
            sizeof(error_msg),
            NULL
        );
        printf("Windows error: %lu - %s\n", win_error, error_msg);
    }
}

void initialize_logging(void) {
    // Initialize basic console logging
    logger_init();
    logger_set_level(LOG_DEBUG);
    logger_enable_console(true);
}

int init_thread_registry(void) {
    ThreadRegistryErrorCode err = thread_registry_init();
    if (err != THREAD_REGISTRY_SUCCESS) {
        printf("Failed to initialize thread registry: %d\n", err);
        return 0;
    }
    return 1;
}

int init_shared_memory_writer(void) {
    printf("Test: Initializing shared memory writer\n");

    void* writer_data = NULL;
    PlatformErrorCode error;

    // Create shared memory
    error = win_shared_memory_open(&g_writer_handle, TEST_SHM_NAME, TEST_SHM_SIZE,
                                  WIN_SHM_READWRITE, true);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("shared memory creation", error);
        return 0;
    }

    // Map shared memory
    error = win_shared_memory_map(g_writer_handle, &writer_data);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory mapping", error);
        win_shared_memory_close(g_writer_handle);
        g_writer_handle = NULL;
        return 0;
    }

    // Initialize with some data
    error = win_shared_memory_lock(g_writer_handle, 5000);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory locking", error);
        win_shared_memory_unmap(g_writer_handle);
        win_shared_memory_close(g_writer_handle);
        g_writer_handle = NULL;
        return 0;
    }

    // Initialize with test data
    const char* init_data = "Initial shared memory data";
    memcpy(writer_data, init_data, strlen(init_data) + 1);

    // Unlock shared memory
    win_shared_memory_unlock(g_writer_handle);

    printf("Successfully initialized shared memory writer\n");
    return 1;
}

int init_shared_memory_sync(void) {
    printf("Test: Initializing shared memory sync\n");

    // Initialize the synchronization system
    PlatformErrorCode error = shared_memory_sync_init("shared_memory_sync.block1");
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("shared memory sync initialization", error);
        return 0;
    }

    // Get the UDP listener thread config and register it
    ThreadConfig* listener_thread = get_udp_listener_thread();
    ThreadRegistryErrorCode reg_error = thread_registry_register(listener_thread);
    if (reg_error != THREAD_REGISTRY_SUCCESS) {
        printf("Failed to register UDP listener thread: %d\n", reg_error);
        shared_memory_sync_shutdown();
        return 0;
    }

    printf("Successfully initialized shared memory sync\n");
    return 1;
}

int start_registered_threads(void) {
    printf("Starting registered threads...\n");
    ThreadRegistryErrorCode err = thread_registry_start_all();
    if (err != THREAD_REGISTRY_SUCCESS) {
        printf("Failed to start threads: %d\n", err);
        return 0;
    }
    printf("All threads started successfully\n");
    return 1;
}

void update_shared_memory(void) {
    if (!g_writer_handle) {
        printf("Writer handle is not initialized\n");
        return;
    }

    void* data;
    PlatformErrorCode error = win_shared_memory_map(g_writer_handle, &data);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory mapping for update", error);
        return;
    }

    error = win_shared_memory_lock(g_writer_handle, 5000);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory locking for update", error);
        win_shared_memory_unmap(g_writer_handle);
        return;
    }

    // Update the memory with a timestamp
    char buffer[256];
    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf(buffer, "Memory updated at %02d:%02d:%02d.%03d", 
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    memcpy(data, buffer, strlen(buffer) + 1);
    
    printf("Updated shared memory: '%s'\n", buffer);

    win_shared_memory_unlock(g_writer_handle);
    win_shared_memory_unmap(g_writer_handle);
}

void cleanup(void) {
    printf("Cleaning up resources...\n");
    
    // Signal shutdown
    shutdown_signal();
    
    // Wait for threads to stop
    sleep_ms(500);
    
    // Clean up shared memory sync
    shared_memory_sync_shutdown();
    
    // Clean up thread registry
    thread_registry_shutdown();
    
    // Clean up shared memory handles
    if (g_writer_handle) {
        win_shared_memory_close(g_writer_handle);
        g_writer_handle = NULL;
    }
    
    if (g_reader_handle) {
        win_shared_memory_close(g_reader_handle);
        g_reader_handle = NULL;
    }
    
    printf("Cleanup complete\n");
}

int main(int argc, char* argv[]) {
    printf("Shared Memory Synchronization Test\n");
    printf("==================================\n\n");

    // Initialize systems
    initialize_logging();
    
    if (!init_thread_registry()) {
        printf("Failed to initialize thread registry\n");
        return 1;
    }
    
    if (!init_shared_memory_writer()) {
        printf("Failed to initialize shared memory writer\n");
        cleanup();
        return 1;
    }
    
    if (!init_shared_memory_sync()) {
        printf("Failed to initialize shared memory sync\n");
        cleanup();
        return 1;
    }
    
    if (!start_registered_threads()) {
        printf("Failed to start threads\n");
        cleanup();
        return 1;
    }
    
    printf("\nTest running. Will update shared memory every 2 seconds.\n");
    printf("Press Ctrl+C to stop...\n\n");
    
    // Main loop - update shared memory periodically
    int iterations = 30;  // Run for 60 seconds
    for (int i = 0; i < iterations && !shutdown_signalled(); i++) {
        update_shared_memory();
        sleep_ms(2000);  // 2 seconds between updates
    }
    
    cleanup();
    return 0;
}
