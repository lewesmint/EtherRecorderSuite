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

// Function prototypes
void print_error(const char* operation, PlatformErrorCode error);
int test_create_and_write();
int test_open_and_read();
int test_concurrent_access();
char* dump_memory_content(void* data, size_t len);

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

// Test reading with various thread access patterns
int test_concurrent_access() {
    printf("Test: Simulating concurrent access with multiple processes\n");
    
    WinSharedMemoryHandle handle = NULL;
    void* data = NULL;
    PlatformErrorCode error;
    int result = 0;

    // Open existing shared memory
    error = win_shared_memory_open(&handle, TEST_SHM_NAME, 0, WIN_SHM_READWRITE, false);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("opening shared memory for concurrent access", error);
        return 0;
    }

    // Map shared memory
    error = win_shared_memory_map(handle, &data);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("mapping memory for concurrent access", error);
        win_shared_memory_close(handle);
        return 0;
    }

    // Lock shared memory
    error = win_shared_memory_lock(handle, 5000);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("locking memory for concurrent access", error);
        win_shared_memory_unmap(handle);
        win_shared_memory_close(handle);
        return 0;
    }

    // Display memory content
    printf("Detailed memory state:\n%s\n", dump_memory_content(data, 64));
    
    // Unlock shared memory
    error = win_shared_memory_unlock(handle);
    if (error != PLATFORM_ERROR_SUCCESS) {
        print_error("unlocking memory for concurrent access", error);
        win_shared_memory_unmap(handle);
        win_shared_memory_close(handle);
        return 0;
    }

    // Unmap and close
    win_shared_memory_unmap(handle);
    win_shared_memory_close(handle);
    
    printf("Successfully tested concurrent access to shared memory\n");
    result = 1;
    return result;
}

// Helper for memory content debugging
char* dump_memory_content(void* data, size_t len) {
    static char buffer[512];
    unsigned char* bytes = (unsigned char*)data;
    int offset = 0;
    
    if (!data) {
        strcpy(buffer, "ERROR: Null data pointer");
        return buffer;
    }
    
    // Format first N bytes as hex + ASCII
    for (size_t i = 0; i < len && offset < 480; i++) {
        offset += sprintf(buffer + offset, "%02X ", bytes[i]);
        if ((i+1) % 16 == 0) {
            offset += sprintf(buffer + offset, " | ");
            for (size_t j = i-15; j <= i; j++) {
                offset += sprintf(buffer + offset, "%c", 
                    (bytes[j] >= 32 && bytes[j] <= 126) ? bytes[j] : '.');
            }
            offset += sprintf(buffer + offset, "\n");
        }
    }
    
    // Add final ASCII representation if we didn't end on a 16-byte boundary
    size_t remainder = len % 16;
    if (remainder > 0) {
        // Add padding spaces for alignment
        for (size_t i = 0; i < (16 - remainder) * 3; i++) {
            offset += sprintf(buffer + offset, " ");
        }
        
        offset += sprintf(buffer + offset, " | ");
        for (size_t j = len - remainder; j < len; j++) {
            offset += sprintf(buffer + offset, "%c", 
                (bytes[j] >= 32 && bytes[j] <= 126) ? bytes[j] : '.');
        }
    }
    
    return buffer;
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

    if (!test_concurrent_access()) {
        printf("Concurrent access test failed\n");
        return 1;
    }

    // Clean up global handle
    if (g_handle) {
        win_shared_memory_close(g_handle);
        g_handle = NULL;
    }

    printf("\nShared memory cleaned up\n");
    return 0;
}
