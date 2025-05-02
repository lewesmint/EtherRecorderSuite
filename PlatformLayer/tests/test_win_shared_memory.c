/**
 * @file test_win_shared_memory.c
 * @brief Test program for Windows shared memory implementation
 */
#include "win_shared_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TEST_SHM_NAME "TestSharedMemory"
#define TEST_SHM_SIZE 1024

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

// Global handle to keep shared memory open between tests
static WinSharedMemoryHandle g_handle = NULL;

int test_create_and_write() {
    printf("Test: Create shared memory and write data\n");

    void* data = NULL;
    PlatformErrorCode error;

    // Create shared memory
    error = win_shared_memory_open(&g_handle, TEST_SHM_NAME, TEST_SHM_SIZE,
                                  WIN_SHM_READWRITE, true);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("shared memory creation", error);
        return 0;
    }

    // Map shared memory
    error = win_shared_memory_map(g_handle, &data);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory mapping", error);
        win_shared_memory_close(g_handle);
        g_handle = NULL;
        return 0;
    }

    // Lock shared memory
    error = win_shared_memory_lock(g_handle, 5000);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory locking", error);
        win_shared_memory_unmap(g_handle);
        win_shared_memory_close(g_handle);
        g_handle = NULL;
        return 0;
    }

    // Write data to shared memory
    const char* test_string = "Hello from shared memory!";
    memcpy(data, test_string, strlen(test_string) + 1);

    // Unlock shared memory
    error = win_shared_memory_unlock(g_handle);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory unlocking", error);
        win_shared_memory_unmap(g_handle);
        win_shared_memory_close(g_handle);
        g_handle = NULL;
        return 0;
    }

    // Unmap but don't close (keep it open for the next test)
    win_shared_memory_unmap(g_handle);

    printf("Successfully created and wrote to shared memory\n");
    return 1;
}

int test_open_and_read() {
    printf("Test: Open existing shared memory and read data\n");

    WinSharedMemoryHandle handle = NULL;
    void* data = NULL;
    PlatformErrorCode error;

    // Open existing shared memory
    error = win_shared_memory_open(&handle, TEST_SHM_NAME, 0,
                                  WIN_SHM_READ, false);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("opening shared memory", error);
        return 0;
    }

    // Map shared memory
    error = win_shared_memory_map(handle, &data);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory mapping", error);
        win_shared_memory_close(handle);
        return 0;
    }

    // Lock shared memory
    error = win_shared_memory_lock(handle, 5000);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory locking", error);
        win_shared_memory_unmap(handle);
        win_shared_memory_close(handle);
        return 0;
    }

    // Read and print data from shared memory
    printf("Data from shared memory: %s\n", (char*)data);

    // Unlock shared memory
    error = win_shared_memory_unlock(handle);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("memory unlocking", error);
        win_shared_memory_unmap(handle);
        win_shared_memory_close(handle);
        return 0;
    }

    // Get size of shared memory
    size_t size;
    error = win_shared_memory_get_size(handle, &size);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("getting memory size", error);
    } else {
        printf("Shared memory size: %zu bytes\n", size);
    }

    // Unmap and close
    win_shared_memory_unmap(handle);
    win_shared_memory_close(handle);

    printf("Successfully opened and read from shared memory\n");
    return 1;
}

int main() {
    printf("Windows Shared Memory Test\n");
    printf("==========================\n\n");

    if (!test_create_and_write()) {
        printf("Create and write test failed\n");
        return 1;
    }

    printf("\nShared memory created and ready for access.\n");
    printf("The shared memory name is 'TestSharedMemory'\n");
    printf("The shared memory contains the string 'Hello from shared memory!'\n");
    printf("The shared memory will remain open for 60 seconds...\n");

    // Keep the shared memory open for 60 seconds
    for (int i = 60; i > 0; i--) {
        printf("\rTime remaining: %d seconds...  ", i);
        Sleep(1000);
    }

    // Clean up global handle
    if (g_handle) {
        win_shared_memory_close(g_handle);
        g_handle = NULL;
    }

    printf("\nShared memory cleaned up\n");
    return 0;
}
