#!/usr/bin/env python
"""
Test script for large shared memory with multiple region changes.
This script creates a 2MB shared memory segment and makes changes to multiple regions
while using a named mutex for protection.
"""

import ctypes
from ctypes import wintypes
import time
import sys
import argparse
import random
import struct

# Windows API constants
INVALID_HANDLE_VALUE = -1
PAGE_READWRITE = 0x04
FILE_MAP_ALL_ACCESS = 0x000F001F

# Load Windows API functions
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

# Define Windows API function prototypes
kernel32.CreateFileMappingA.argtypes = [
    wintypes.HANDLE,    # hFile
    wintypes.LPVOID,    # lpFileMappingAttributes
    wintypes.DWORD,     # flProtect
    wintypes.DWORD,     # dwMaximumSizeHigh
    wintypes.DWORD,     # dwMaximumSizeLow
    wintypes.LPCSTR     # lpName
]
kernel32.CreateFileMappingA.restype = wintypes.HANDLE

kernel32.MapViewOfFile.argtypes = [
    wintypes.HANDLE,    # hFileMappingObject
    wintypes.DWORD,     # dwDesiredAccess
    wintypes.DWORD,     # dwFileOffsetHigh
    wintypes.DWORD,     # dwFileOffsetLow
    ctypes.c_size_t     # dwNumberOfBytesToMap
]
kernel32.MapViewOfFile.restype = wintypes.LPVOID

kernel32.UnmapViewOfFile.argtypes = [wintypes.LPCVOID]
kernel32.UnmapViewOfFile.restype = wintypes.BOOL

kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL

kernel32.CreateMutexA.argtypes = [
    wintypes.LPVOID,    # lpMutexAttributes
    wintypes.BOOL,      # bInitialOwner
    wintypes.LPCSTR     # lpName
]
kernel32.CreateMutexA.restype = wintypes.HANDLE

kernel32.WaitForSingleObject.argtypes = [
    wintypes.HANDLE,    # hHandle
    wintypes.DWORD      # dwMilliseconds
]
kernel32.WaitForSingleObject.restype = wintypes.DWORD

kernel32.ReleaseMutex.argtypes = [wintypes.HANDLE]
kernel32.ReleaseMutex.restype = wintypes.BOOL

def create_shared_memory(name, size):
    """Create a new shared memory segment."""
    try:
        print(f"Creating shared memory '{name}' with size {size:,} bytes ({size / (1024*1024):.2f} MB)")
        
        # Split size into high and low DWORDs
        size_high = (size >> 32) & 0xFFFFFFFF
        size_low = size & 0xFFFFFFFF
        
        print(f"Size high DWORD: {size_high}, Size low DWORD: {size_low}")
        
        handle = kernel32.CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            None,
            PAGE_READWRITE,
            size_high,
            size_low,
            name.encode('ascii')
        )

        if not handle:
            error = ctypes.get_last_error()
            error_msg = ctypes.WinError(error)
            print(f"CreateFileMappingA failed with error code {error}: {error_msg}")
            raise error_msg

        print(f"Successfully created shared memory with handle: {handle}")
        return handle
    except Exception as e:
        print(f"Unexpected error in create_shared_memory: {e}")
        import traceback
        traceback.print_exc()
        raise

def map_shared_memory(handle):
    """Map shared memory into the process address space."""
    try:
        print(f"Attempting to map shared memory with handle: {handle}")
        address = kernel32.MapViewOfFile(
            handle,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            0  # Map the entire file
        )

        if not address:
            error = ctypes.get_last_error()
            error_msg = ctypes.WinError(error)
            print(f"MapViewOfFile failed with error code {error}: {error_msg}")
            raise error_msg

        print(f"Successfully mapped shared memory at address: 0x{address:X}")
        
        # Get memory information to report actual size
        try:
            class MEMORY_BASIC_INFORMATION(ctypes.Structure):
                _fields_ = [
                    ("BaseAddress", ctypes.c_void_p),
                    ("AllocationBase", ctypes.c_void_p),
                    ("AllocationProtect", ctypes.c_ulong),
                    ("RegionSize", ctypes.c_size_t),
                    ("State", ctypes.c_ulong),
                    ("Protect", ctypes.c_ulong),
                    ("Type", ctypes.c_ulong)
                ]
            
            mbi = MEMORY_BASIC_INFORMATION()
            
            # Convert address to c_void_p to handle 64-bit addresses properly
            address_ptr = ctypes.c_void_p(address)
            
            result = kernel32.VirtualQuery(
                address_ptr,
                ctypes.byref(mbi),
                ctypes.sizeof(MEMORY_BASIC_INFORMATION)
            )
            
            if result:
                print(f"Actual allocated memory region size: {mbi.RegionSize:,} bytes ({mbi.RegionSize / (1024*1024):.2f} MB)")
            else:
                error = ctypes.get_last_error()
                print(f"VirtualQuery failed with error code {error}: {ctypes.WinError(error)}")
        except Exception as e:
            print(f"Error querying memory information: {e}")
            import traceback
            traceback.print_exc()
        
        return address
    except Exception as e:
        print(f"Unexpected error in map_shared_memory: {e}")
        import traceback
        traceback.print_exc()
        raise

def unmap_shared_memory(address):
    """Unmap shared memory from the process address space."""
    if not kernel32.UnmapViewOfFile(address):
        error = ctypes.get_last_error()
        raise ctypes.WinError(error)

def close_handle(handle):
    """Close a handle."""
    if not kernel32.CloseHandle(handle):
        error = ctypes.get_last_error()
        raise ctypes.WinError(error)

def create_mutex(name):
    """Create a named mutex."""
    handle = kernel32.CreateMutexA(
        None,
        False,
        name.encode('ascii')
    )

    if not handle:
        error = ctypes.get_last_error()
        raise ctypes.WinError(error)

    return handle

def lock_mutex(handle, timeout_ms=5000):
    """Lock a mutex."""
    result = kernel32.WaitForSingleObject(handle, timeout_ms)

    if result == 0x00000000:  # WAIT_OBJECT_0
        return True
    elif result == 0x00000102:  # WAIT_TIMEOUT
        return False
    else:
        error = ctypes.get_last_error()
        raise ctypes.WinError(error)

def unlock_mutex(handle):
    """Unlock a mutex."""
    if not kernel32.ReleaseMutex(handle):
        error = ctypes.get_last_error()
        raise ctypes.WinError(error)

def modify_memory_regions(address, size, num_regions=5):
    """Modify multiple regions of shared memory."""
    # Create regions of different sizes
    regions = []
    
    # Add a few single-byte changes
    num_single_bytes = 3  # Number of single-byte changes to make
    for _ in range(num_single_bytes):
        offset = random.randint(0, size - 1)
        regions.append((offset, 1))  # Length of 1 byte
    
    # Add multi-byte regions
    for _ in range(num_regions - num_single_bytes):
        # Random offset and length
        offset = random.randint(0, size - 1024)  # Ensure at least 1KB space
        length = random.randint(64, 1024)  # Between 64 bytes and 1KB
        
        # Make sure we don't go beyond the memory size
        if offset + length > size:
            length = size - offset
            
        regions.append((offset, length))
    
    # Modify each region with unique data
    for i, (offset, length) in enumerate(regions):
        if length == 1:
            # For single-byte changes, read current value and ensure change
            current_value = ctypes.c_ubyte.from_address(address + offset).value
            new_value = current_value
            while new_value == current_value:
                new_value = random.randint(0, 255)
            
            # Write the new value
            ctypes.memset(address + offset, new_value, 1)
            print(f"Modified single byte {i}: offset={offset}, old={current_value}, new={new_value}")
        else:
            # For multi-byte regions, create a buffer to store current data
            current_data = bytearray(length)
            for j in range(length):
                current_data[j] = ctypes.c_ubyte.from_address(address + offset + j).value
            
            # Create new data that's guaranteed to be different
            new_data = bytearray(length)
            for j in range(length):
                # Ensure each byte is different from the current value
                new_byte = current_data[j]
                # Increment by a random value between 1 and 255 to ensure change
                new_byte = (new_byte + random.randint(1, 255)) % 256
                new_data[j] = new_byte
            
            # Write the new data
            for j in range(length):
                ctypes.c_ubyte.from_address(address + offset + j).value = new_data[j]
            
            print(f"Modified region {i}: offset={offset}, length={length}, all bytes changed")
    
    return regions

def run_test(name, size, cycles, interval, regions_per_cycle):
    """Run the shared memory test."""
    try:
        # Create shared memory
        shm_handle = create_shared_memory(name, size)
        print("Shared memory created successfully")
        
        # Create mutex
        mutex_name = f"Mutex_{name}"
        mutex_handle = create_mutex(mutex_name)
        print(f"Mutex '{mutex_name}' created successfully")
        
        # Map shared memory
        address = map_shared_memory(shm_handle)
        print("Shared memory mapped successfully")
        
        # Initialize memory with zeros
        ctypes.memset(address, 0, size)
        print("Memory initialized with zeros")
        
        # Run test cycles
        print(f"Starting {cycles} test cycles with {interval}s interval")
        for cycle in range(cycles):
            print(f"\nCycle {cycle+1}/{cycles}")
            
            # Lock mutex
            if lock_mutex(mutex_handle):
                print("Mutex locked successfully")
                
                # Modify multiple regions
                regions = modify_memory_regions(address, size, regions_per_cycle)
                
                # Unlock mutex
                unlock_mutex(mutex_handle)
                print("Mutex unlocked successfully")
                
                # Wait for the next cycle
                if cycle < cycles - 1:
                    print(f"Waiting {interval} seconds before next cycle...")
                    time.sleep(interval)
            else:
                print("Failed to lock mutex (timeout)")
                break
        
        # Cleanup
        unmap_shared_memory(address)
        close_handle(shm_handle)
        close_handle(mutex_handle)
        print("Resources cleaned up successfully")
        
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    """Main function."""
    parser = argparse.ArgumentParser(
        description='Large Shared Memory Test with Multiple Region Changes'
    )
    parser.add_argument('--name', default='MySharedMemory',
                        help='Name of the shared memory segment (default: MySharedMemory)')
    parser.add_argument('--size', type=int, default=int(1.26 * 1024 * 1024),
                        help='Size of the shared memory segment in bytes (default: 1.25MB)')
    parser.add_argument('--cycles', type=int, default=0,
                        help='Number of test cycles to run (0 = run indefinitely)')
    parser.add_argument('--interval', type=float, default=5.0,
                        help='Interval between cycles in seconds (default: 5.0)')
    parser.add_argument('--regions', type=int, default=5,
                        help='Number of regions to modify in each cycle')
    
    args = parser.parse_args()
    
    try:
        # Create shared memory
        shm_handle = create_shared_memory(args.name, args.size)
        address = map_shared_memory(shm_handle)
        
        # Initialize memory with zeros
        print("Initializing memory with zeros")
        ctypes.memset(address, 0, args.size)
        
        # Create mutex
        mutex_name = f"Mutex_{args.name}"
        print(f"Creating mutex '{mutex_name}'")
        mutex_handle = create_mutex(mutex_name)
        
        # Run test cycles
        cycle = 1
        run_indefinitely = (args.cycles == 0)
        
        print(f"Starting test cycles with {args.interval}s interval")
        print("Press Ctrl+C to stop")
        
        while run_indefinitely or cycle <= args.cycles:
            print(f"\nCycle {cycle}")
            
            # Lock mutex
            if lock_mutex(mutex_handle):
                print("Mutex locked successfully")
                
                # Modify multiple regions
                regions = modify_memory_regions(address, args.size, args.regions)
                
                # Unlock mutex
                unlock_mutex(mutex_handle)
                print("Mutex unlocked successfully")
                
                # Wait for the next cycle
                print(f"Waiting {args.interval} seconds before next cycle...")
                time.sleep(args.interval)
                cycle += 1
            else:
                print("Failed to lock mutex (timeout)")
                break
        
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        # Cleanup
        try:
            print("Cleaning up resources")
            unmap_shared_memory(address)
            close_handle(shm_handle)
            close_handle(mutex_handle)
            print("Resources cleaned up successfully")
        except Exception as e:
            print(f"Error during cleanup: {e}")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())














