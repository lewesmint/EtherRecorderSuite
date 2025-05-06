#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define SHM_NAME "MySharedMemory"
#define SHM_SIZE (1024 * 1024 * 1.2) // 1.2 MB

int main() {
    HANDLE hMapFile;
    LPVOID pBuf;
    MEMORY_BASIC_INFORMATION memInfo;
    
    printf("Creating shared memory of size: %d bytes\n", (int)SHM_SIZE);
    
    // Create file mapping
    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // Use paging file
        NULL,                    // Default security
        PAGE_READWRITE,          // Read/write access
        0,                       // Maximum object size (high-order DWORD)
        SHM_SIZE,                // Maximum object size (low-order DWORD)
        SHM_NAME);               // Name of mapping object
    
    if (hMapFile == NULL) {
        printf("Could not create file mapping object (%d).\n", GetLastError());
        return 1;
    }
    
    // Map view of file
    pBuf = MapViewOfFile(
        hMapFile,                // Handle to map object
        FILE_MAP_ALL_ACCESS,     // Read/write permission
        0,                       // High-order DWORD of offset
        0,                       // Low-order DWORD of offset
        SHM_SIZE);               // Number of bytes to map
    
    if (pBuf == NULL) {
        printf("Could not map view of file (%d).\n", GetLastError());
        CloseHandle(hMapFile);
        return 1;
    }
    
    // Query memory information to get actual size
    if (VirtualQuery(pBuf, &memInfo, sizeof(memInfo))) {
        printf("Shared memory region size: %zu bytes\n", memInfo.RegionSize);
        printf("Memory protection: 0x%lX\n", memInfo.Protect);
    } else {
        printf("Failed to query memory information (%d).\n", GetLastError());
    }
    
    // Write to shared memory
    strcpy_s((CHAR*)pBuf, SHM_SIZE, "Hello from shared memory!");
    printf("Data written to shared memory: %s\n", (CHAR*)pBuf);
    
    // Keep program running so the other process can access the shared memory
    printf("Press Enter to exit...\n");
    getchar();
    
    // Unmap and close
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    
    return 0;
}
