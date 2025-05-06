#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winternl.h>

#define SHM_NAME "MySharedMemory"

// Define the section information class for NtQuerySection
typedef enum _SECTION_INFORMATION_CLASS {
    SectionBasicInformation,
    SectionImageInformation,
    SectionRelocationInformation,
    MaxSectionInfoClass
} SECTION_INFORMATION_CLASS;

// Define the section basic information structure
typedef struct _SECTION_BASIC_INFORMATION {
    PVOID BaseAddress;
    ULONG AllocationAttributes;
    LARGE_INTEGER MaximumSize;
} SECTION_BASIC_INFORMATION, *PSECTION_BASIC_INFORMATION;

// Define the function prototype for NtQuerySection
typedef NTSTATUS (NTAPI *PFN_NTQUERYSECTION)(
    HANDLE SectionHandle,
    SECTION_INFORMATION_CLASS InformationClass,
    PVOID InformationBuffer,
    ULONG InformationBufferSize,
    PULONG ResultLength
);

int main() {
    HANDLE hMapFile;
    LPVOID pBuf;
    SIZE_T actual_size;
    HMODULE hNtDll;
    PFN_NTQUERYSECTION NtQuerySection;
    SECTION_BASIC_INFORMATION sbi;
    ULONG returnLength;
    NTSTATUS status;
    MEMORY_BASIC_INFORMATION memInfo;
    
    printf("Opening shared memory '%s'\n", SHM_NAME);
    
    // Open existing file mapping
    hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,     // Full access
        FALSE,                   // Do not inherit the name
        SHM_NAME);               // Name of mapping object
    
    if (hMapFile == NULL) {
        printf("Could not open file mapping object (%d).\n", GetLastError());
        printf("Is the creator program running?\n");
        return 1;
    }
    
    // Load ntdll.dll to get access to NtQuerySection
    hNtDll = LoadLibrary("ntdll.dll");
    if (hNtDll == NULL) {
        printf("Could not load ntdll.dll (%d).\n", GetLastError());
        CloseHandle(hMapFile);
        return 1;
    }
    
    // Get the address of NtQuerySection
    NtQuerySection = (PFN_NTQUERYSECTION)GetProcAddress(hNtDll, "NtQuerySection");
    if (NtQuerySection == NULL) {
        printf("Could not get address of NtQuerySection (%d).\n", GetLastError());
        FreeLibrary(hNtDll);
        CloseHandle(hMapFile);
        return 1;
    }
    
    // Query section information to get the actual size
    ZeroMemory(&sbi, sizeof(sbi));
    status = NtQuerySection(
        hMapFile,
        SectionBasicInformation,
        &sbi,
        sizeof(sbi),
        &returnLength
    );
    
    if (status != 0) {
        printf("NtQuerySection failed with status 0x%lX.\n", status);
        printf("Falling back to VirtualQuery method...\n");
        
        // Map with a small initial size for VirtualQuery
        pBuf = MapViewOfFile(
            hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            4096  // Small initial size
        );
        
        if (pBuf == NULL) {
            printf("Could not map view of file (%d).\n", GetLastError());
            FreeLibrary(hNtDll);
            CloseHandle(hMapFile);
            return 1;
        }
        
        // Query memory information to get actual size
        if (VirtualQuery(pBuf, &memInfo, sizeof(memInfo))) {
            actual_size = memInfo.RegionSize;
            printf("VirtualQuery detected size: %zu bytes\n", actual_size);
            
            // Unmap the initial view
            UnmapViewOfFile(pBuf);
        } else {
            printf("VirtualQuery failed (%d).\n", GetLastError());
            UnmapViewOfFile(pBuf);
            FreeLibrary(hNtDll);
            CloseHandle(hMapFile);
            return 1;
        }
    } else {
        // Get the actual size from the section information
        actual_size = (SIZE_T)sbi.MaximumSize.QuadPart;
        printf("NtQuerySection detected size: %zu bytes\n", actual_size);
        printf("Allocation attributes: 0x%lX\n", sbi.AllocationAttributes);
    }
    
    // Map view of file with the actual size
    pBuf = MapViewOfFile(
        hMapFile,                // Handle to map object
        FILE_MAP_ALL_ACCESS,     // Full access
        0,                       // High-order DWORD of offset
        0,                       // Low-order DWORD of offset
        actual_size);            // Number of bytes to map
    
    if (pBuf == NULL) {
        printf("Could not map view of file (%d).\n", GetLastError());
        FreeLibrary(hNtDll);
        CloseHandle(hMapFile);
        return 1;
    }
    
    // Read from shared memory
    printf("Data read from shared memory: %s\n", (CHAR*)pBuf);
    
    // Comment out the write operation
    // strcpy_s((CHAR*)pBuf, actual_size, "Hello from C reader!");
    // printf("Data written to shared memory: %s\n", (CHAR*)pBuf);
    
    // Keep program running for testing
    printf("Press Enter to exit...\n");
    getchar();
    
    // Unmap and close
    UnmapViewOfFile(pBuf);
    FreeLibrary(hNtDll);
    CloseHandle(hMapFile);
    
    return 0;
}

