#!/usr/bin/env python
"""
Python test script for Windows shared memory functionality.
This script provides an independent verification of the shared memory implementation.
"""

import ctypes
from ctypes import wintypes
import time
import sys
import argparse

# Windows API constants
INVALID_HANDLE_VALUE = -1
FILE_MAP_READ = 0x0004
FILE_MAP_WRITE = 0x0002
FILE_MAP_ALL_ACCESS = 0x001F
PAGE_READWRITE = 0x04

# Load Windows DLLs
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

kernel32.OpenFileMappingA.argtypes = [
    wintypes.DWORD,     # dwDesiredAccess
    wintypes.BOOL,      # bInheritHandle
    wintypes.LPCSTR     # lpName
]
kernel32.OpenFileMappingA.restype = wintypes.HANDLE

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
    handle = kernel32.CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        None,
        PAGE_READWRITE,
        0,
        size,
        name.encode('ascii')
    )
    
    if not handle:
        error = ctypes.get_last_error()
        raise ctypes.WinError(error)
    
    return handle

def open_shared_memory(name, read_only=False):
    """Open an existing shared memory segment."""
    access = FILE_MAP_READ if read_only else FILE_MAP_ALL_ACCESS
    
    handle = kernel32.OpenFileMappingA(
        access,
        False,
        name.encode('ascii')
    )
    
    if not handle:
        error = ctypes.get_last_error()
        raise ctypes.WinError(error)
    
    return handle

def map_shared_memory(handle, size=0, read_only=False):
    """Map a shared memory segment into the process address space."""
    access = FILE_MAP_READ if read_only else FILE_MAP_ALL_ACCESS
    
    address = kernel32.MapViewOfFile(
        handle,
        access,
        0,
        0,
        size  # 0 means map the entire file
    )
    
    if not address:
        error = ctypes.get_last_error()
        raise ctypes.WinError(error)
    
    return address

def unmap_shared_memory(address):
    """Unmap a shared memory segment."""
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

def write_to_shared_memory(address, data, size):
    """Write data to shared memory."""
    buffer = ctypes.create_string_buffer(data.encode('ascii'), size)
    ctypes.memmove(address, buffer, size)

def read_from_shared_memory(address, size):
    """Read data from shared memory."""
    buffer = ctypes.create_string_buffer(size)
    ctypes.memmove(buffer, address, size)
    return buffer.value.decode('ascii')

def create_and_write(name, size, data):
    """Create shared memory and write data to it."""
    print(f"Creating shared memory '{name}' with size {size} bytes")
    
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
        
        # Lock mutex
        if lock_mutex(mutex_handle):
            print("Mutex locked successfully")
            
            # Write data
            write_to_shared_memory(address, data, len(data) + 1)  # +1 for null terminator
            print(f"Data written to shared memory: '{data}'")
            
            # Unlock mutex
            unlock_mutex(mutex_handle)
            print("Mutex unlocked successfully")
        else:
            print("Failed to lock mutex (timeout)")
        
        # Cleanup
        unmap_shared_memory(address)
        close_handle(shm_handle)
        close_handle(mutex_handle)
        print("Resources cleaned up successfully")
        
        return True
    
    except Exception as e:
        print(f"Error: {e}")
        return False

def open_and_read(name, size):
    """Open shared memory and read data from it."""
    print(f"Opening shared memory '{name}'")
    
    try:
        # Open shared memory
        shm_handle = open_shared_memory(name)
        print("Shared memory opened successfully")
        
        # Open mutex
        mutex_name = f"Mutex_{name}"
        mutex_handle = create_mutex(mutex_name)
        print(f"Mutex '{mutex_name}' opened successfully")
        
        # Map shared memory
        address = map_shared_memory(shm_handle)
        print("Shared memory mapped successfully")
        
        # Lock mutex
        if lock_mutex(mutex_handle):
            print("Mutex locked successfully")
            
            # Read data
            data = read_from_shared_memory(address, size)
            print(f"Data read from shared memory: '{data}'")
            
            # Unlock mutex
            unlock_mutex(mutex_handle)
            print("Mutex unlocked successfully")
        else:
            print("Failed to lock mutex (timeout)")
            data = None
        
        # Cleanup
        unmap_shared_memory(address)
        close_handle(shm_handle)
        close_handle(mutex_handle)
        print("Resources cleaned up successfully")
        
        return data
    
    except Exception as e:
        print(f"Error: {e}")
        return None

def monitor_shared_memory(name, size, interval=1.0, duration=60.0):
    """Monitor shared memory for changes."""
    print(f"Monitoring shared memory '{name}' for changes")
    
    try:
        # Open shared memory
        shm_handle = open_shared_memory(name)
        print("Shared memory opened successfully")
        
        # Open mutex
        mutex_name = f"Mutex_{name}"
        mutex_handle = create_mutex(mutex_name)
        print(f"Mutex '{mutex_name}' opened successfully")
        
        # Map shared memory
        address = map_shared_memory(shm_handle)
        print("Shared memory mapped successfully")
        
        # Monitor for changes
        last_data = None
        start_time = time.time()
        
        while time.time() - start_time < duration:
            # Lock mutex
            if lock_mutex(mutex_handle, 100):  # Short timeout to avoid blocking
                # Read data
                data = read_from_shared_memory(address, size)
                
                # Check for changes
                if data != last_data:
                    print(f"Data changed: '{data}'")
                    last_data = data
                
                # Unlock mutex
                unlock_mutex(mutex_handle)
            
            time.sleep(interval)
        
        # Cleanup
        unmap_shared_memory(address)
        close_handle(shm_handle)
        close_handle(mutex_handle)
        print("Resources cleaned up successfully")
        
        return True
    
    except Exception as e:
        print(f"Error: {e}")
        return False

def main():
    """Main function."""
    parser = argparse.ArgumentParser(description='Windows Shared Memory Test')
    parser.add_argument('--mode', choices=['create', 'read', 'monitor'], required=True,
                        help='Operation mode')
    parser.add_argument('--name', default='TestSharedMemory',
                        help='Name of the shared memory segment')
    parser.add_argument('--size', type=int, default=1024,
                        help='Size of the shared memory segment in bytes')
    parser.add_argument('--data', default='Hello from Python!',
                        help='Data to write to shared memory (create mode only)')
    parser.add_argument('--interval', type=float, default=1.0,
                        help='Monitoring interval in seconds (monitor mode only)')
    parser.add_argument('--duration', type=float, default=60.0,
                        help='Monitoring duration in seconds (monitor mode only)')
    
    args = parser.parse_args()
    
    if args.mode == 'create':
        if create_and_write(args.name, args.size, args.data):
            return 0
        return 1
    
    elif args.mode == 'read':
        data = open_and_read(args.name, args.size)
        if data is not None:
            return 0
        return 1
    
    elif args.mode == 'monitor':
        if monitor_shared_memory(args.name, args.size, args.interval, args.duration):
            return 0
        return 1

if __name__ == '__main__':
    sys.exit(main())
