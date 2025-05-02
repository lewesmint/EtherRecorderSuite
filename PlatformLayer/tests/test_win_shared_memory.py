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
import json
import os
import textwrap

# Use standard JSON for parsing JSONC (JSON with Comments)
# We'll strip comments before parsing
import re

def strip_comments(json_str):
    """Strip C-style comments from a JSON string."""
    # First, handle // comments (remove everything from // to end of line)
    result = re.sub(r'//.*?$', '', json_str, flags=re.MULTILINE)

    # Then handle /* */ comments (remove everything between /* and */)
    result = re.sub(r'/\*.*?\*/', '', result, flags=re.DOTALL)

    return result


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

def get_shared_memory_size(address):
    """Get the size of a shared memory segment."""
    # Define the MEMORY_BASIC_INFORMATION structure
    class MEMORY_BASIC_INFORMATION(ctypes.Structure):
        _fields_ = [
            ("BaseAddress", ctypes.c_void_p),
            ("AllocationBase", ctypes.c_void_p),
            ("AllocationProtect", wintypes.DWORD),
            ("RegionSize", ctypes.c_size_t),
            ("State", wintypes.DWORD),
            ("Protect", wintypes.DWORD),
            ("Type", wintypes.DWORD)
        ]

    # Define VirtualQuery function prototype
    kernel32.VirtualQuery.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(MEMORY_BASIC_INFORMATION),
        ctypes.c_size_t
    ]
    kernel32.VirtualQuery.restype = ctypes.c_size_t

    # Query memory information
    mbi = MEMORY_BASIC_INFORMATION()
    if kernel32.VirtualQuery(address, ctypes.byref(mbi), ctypes.sizeof(mbi)) == 0:
        error = ctypes.get_last_error()
        raise ctypes.WinError(error)

    return mbi.RegionSize

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
        try:
            shm_handle = create_shared_memory(name, size)
            print("Shared memory created successfully")
        except Exception as e:
            print(f"ERROR: Failed to create shared memory: {e}")
            print("This could be due to insufficient permissions or a name conflict.")
            import traceback
            traceback.print_exc()
            return False

        # Create mutex
        try:
            mutex_name = f"Mutex_{name}"
            mutex_handle = create_mutex(mutex_name)
            print(f"Mutex '{mutex_name}' created successfully")
        except Exception as e:
            print(f"ERROR: Failed to create mutex: {e}")
            print("Closing shared memory handle and exiting.")
            close_handle(shm_handle)
            import traceback
            traceback.print_exc()
            return False

        # Map shared memory
        try:
            address = map_shared_memory(shm_handle)
            print("Shared memory mapped successfully")
        except Exception as e:
            print(f"ERROR: Failed to map shared memory: {e}")
            close_handle(shm_handle)
            close_handle(mutex_handle)
            import traceback
            traceback.print_exc()
            return False

        # Lock mutex
        if lock_mutex(mutex_handle):
            print("Mutex locked successfully")

            # Write data
            try:
                write_to_shared_memory(address, data, len(data) + 1)  # +1 for null terminator
                print(f"Data written to shared memory: '{data}'")
            except Exception as e:
                print(f"ERROR: Failed to write to shared memory: {e}")
                unlock_mutex(mutex_handle)
                unmap_shared_memory(address)
                close_handle(shm_handle)
                close_handle(mutex_handle)
                import traceback
                traceback.print_exc()
                return False

            # Unlock mutex
            try:
                unlock_mutex(mutex_handle)
                print("Mutex unlocked successfully")
            except Exception as e:
                print(f"WARNING: Failed to unlock mutex: {e}")
                import traceback
                traceback.print_exc()
        else:
            print("Failed to lock mutex (timeout)")

        # Cleanup
        try:
            unmap_shared_memory(address)
            close_handle(shm_handle)
            close_handle(mutex_handle)
            print("Resources cleaned up successfully")
        except Exception as e:
            print(f"WARNING: Error during cleanup: {e}")
            import traceback
            traceback.print_exc()

        return True

    except Exception as e:
        print(f"CRITICAL ERROR: Unexpected exception: {e}")
        import traceback
        traceback.print_exc()
        return False

def open_and_read(name, size=None, max_retries=5, retry_interval=2):
    """Open shared memory and read data from it."""
    print(f"Opening shared memory '{name}'")

    for attempt in range(max_retries):
        try:
            # Open shared memory
            try:
                shm_handle = open_shared_memory(name)
                print("Shared memory opened successfully")
            except Exception as e:
                if attempt < max_retries - 1:
                    print(f"ERROR: Failed to open shared memory: {e}")
                    print(f"This usually means the shared memory segment '{name}' doesn't exist or has been closed.")
                    print(f"Retrying in {retry_interval} seconds... (Attempt {attempt + 1}/{max_retries})")
                    time.sleep(retry_interval)
                    continue
                else:
                    print(f"ERROR: Failed to open shared memory after {max_retries} attempts: {e}")
                    print("Make sure the C program that creates the shared memory is running.")
                    import traceback
                    traceback.print_exc()

                    # Ask user if they want to retry
                    try:
                        response = input("Would you like to retry? (y/n): ").strip().lower()
                        if response == 'y' or response == 'yes':
                            print(f"Retrying to open shared memory '{name}'...")
                            return open_and_read(name, size, max_retries, retry_interval)
                    except KeyboardInterrupt:
                        print("\nOperation cancelled by user.")

                    return None

            # Open mutex
            try:
                mutex_name = f"Mutex_{name}"
                mutex_handle = create_mutex(mutex_name)
                print(f"Mutex '{mutex_name}' opened successfully")
            except Exception as e:
                if attempt < max_retries - 1:
                    print(f"ERROR: Failed to open mutex: {e}")
                    print("Closing shared memory handle and retrying...")
                    close_handle(shm_handle)
                    time.sleep(retry_interval)
                    continue
                else:
                    print(f"ERROR: Failed to open mutex after {max_retries} attempts: {e}")
                    print("Closing shared memory handle and exiting.")
                    close_handle(shm_handle)
                    import traceback
                    traceback.print_exc()
                    return None

            # Map shared memory
            try:
                address = map_shared_memory(shm_handle)
                print("Shared memory mapped successfully")

                # Determine the size of the shared memory if not provided
                if size is None:
                    try:
                        size = get_shared_memory_size(address)
                        print(f"Automatically determined shared memory size: {size} bytes")
                    except Exception as e:
                        print(f"WARNING: Failed to determine shared memory size: {e}")
                        print("Using default size of 4096 bytes")
                        size = 4096
            except Exception as e:
                if attempt < max_retries - 1:
                    print(f"ERROR: Failed to map shared memory: {e}")
                    print("Closing handles and retrying...")
                    close_handle(shm_handle)
                    close_handle(mutex_handle)
                    time.sleep(retry_interval)
                    continue
                else:
                    print(f"ERROR: Failed to map shared memory after {max_retries} attempts: {e}")
                    close_handle(shm_handle)
                    close_handle(mutex_handle)
                    import traceback
                    traceback.print_exc()
                    return None

            # Lock mutex
            if lock_mutex(mutex_handle):
                print("Mutex locked successfully")

                # Read data
                try:
                    data = read_from_shared_memory(address, size)
                    print(f"Data read from shared memory: '{data}'")
                except Exception as e:
                    if attempt < max_retries - 1:
                        print(f"ERROR: Failed to read from shared memory: {e}")
                        print("Unlocking mutex, closing handles, and retrying...")
                        unlock_mutex(mutex_handle)
                        unmap_shared_memory(address)
                        close_handle(shm_handle)
                        close_handle(mutex_handle)
                        time.sleep(retry_interval)
                        continue
                    else:
                        print(f"ERROR: Failed to read from shared memory after {max_retries} attempts: {e}")
                        unlock_mutex(mutex_handle)
                        unmap_shared_memory(address)
                        close_handle(shm_handle)
                        close_handle(mutex_handle)
                        import traceback
                        traceback.print_exc()
                        return None

                # Unlock mutex
                try:
                    unlock_mutex(mutex_handle)
                    print("Mutex unlocked successfully")
                except Exception as e:
                    print(f"WARNING: Failed to unlock mutex: {e}")
                    import traceback
                    traceback.print_exc()
            else:
                if attempt < max_retries - 1:
                    print("Failed to lock mutex (timeout)")
                    print("Closing handles and retrying...")
                    unmap_shared_memory(address)
                    close_handle(shm_handle)
                    close_handle(mutex_handle)
                    time.sleep(retry_interval)
                    continue
                else:
                    print(f"Failed to lock mutex after {max_retries} attempts")
                    unmap_shared_memory(address)
                    close_handle(shm_handle)
                    close_handle(mutex_handle)
                    return None

            # Cleanup
            try:
                unmap_shared_memory(address)
                close_handle(shm_handle)
                close_handle(mutex_handle)
                print("Resources cleaned up successfully")
            except Exception as e:
                print(f"WARNING: Error during cleanup: {e}")
                import traceback
                traceback.print_exc()

            return data

        except Exception as e:
            if attempt < max_retries - 1:
                print(f"CRITICAL ERROR: Unexpected exception: {e}")
                print(f"Retrying in {retry_interval} seconds... (Attempt {attempt + 1}/{max_retries})")
                import traceback
                traceback.print_exc()
                time.sleep(retry_interval)
            else:
                print(f"CRITICAL ERROR: Unexpected exception after {max_retries} attempts: {e}")
                import traceback
                traceback.print_exc()
                return None

    print(f"Failed to open and read shared memory '{name}' after {max_retries} attempts")
    return None

def monitor_shared_memory(name, size=None, interval=1.0, duration=60.0, max_retries=5, retry_interval=2):
    """Monitor shared memory for changes."""
    print(f"Monitoring shared memory '{name}' for changes")

    for attempt in range(max_retries):
        try:
            # Open shared memory
            try:
                shm_handle = open_shared_memory(name)
                print("Shared memory opened successfully")
            except Exception as e:
                if attempt < max_retries - 1:
                    print(f"ERROR: Failed to open shared memory: {e}")
                    print(f"This usually means the shared memory segment '{name}' doesn't exist or has been closed.")
                    print(f"Retrying in {retry_interval} seconds... (Attempt {attempt + 1}/{max_retries})")
                    time.sleep(retry_interval)
                    continue
                else:
                    print(f"ERROR: Failed to open shared memory after {max_retries} attempts: {e}")
                    print("Make sure the C program that creates the shared memory is running.")
                    import traceback
                    traceback.print_exc()

                    # Ask user if they want to retry
                    try:
                        response = input("Would you like to retry? (y/n): ").strip().lower()
                        if response == 'y' or response == 'yes':
                            print(f"Retrying to monitor shared memory '{name}'...")
                            return monitor_shared_memory(name, size, interval, duration, max_retries, retry_interval)
                    except KeyboardInterrupt:
                        print("\nOperation cancelled by user.")

                    return False

            # Open mutex
            try:
                mutex_name = f"Mutex_{name}"
                mutex_handle = create_mutex(mutex_name)
                print(f"Mutex '{mutex_name}' opened successfully")
            except Exception as e:
                if attempt < max_retries - 1:
                    print(f"ERROR: Failed to open mutex: {e}")
                    print("Closing shared memory handle and retrying...")
                    close_handle(shm_handle)
                    time.sleep(retry_interval)
                    continue
                else:
                    print(f"ERROR: Failed to open mutex after {max_retries} attempts: {e}")
                    print("Closing shared memory handle and exiting.")
                    close_handle(shm_handle)
                    import traceback
                    traceback.print_exc()
                    return False

            # Map shared memory
            try:
                address = map_shared_memory(shm_handle)
                print("Shared memory mapped successfully")

                # Determine the size of the shared memory if not provided
                if size is None:
                    try:
                        size = get_shared_memory_size(address)
                        print(f"Automatically determined shared memory size: {size} bytes")
                    except Exception as e:
                        print(f"WARNING: Failed to determine shared memory size: {e}")
                        print("Using default size of 4096 bytes")
                        size = 4096
            except Exception as e:
                if attempt < max_retries - 1:
                    print(f"ERROR: Failed to map shared memory: {e}")
                    print("Closing handles and retrying...")
                    close_handle(shm_handle)
                    close_handle(mutex_handle)
                    time.sleep(retry_interval)
                    continue
                else:
                    print(f"ERROR: Failed to map shared memory after {max_retries} attempts: {e}")
                    close_handle(shm_handle)
                    close_handle(mutex_handle)
                    import traceback
                    traceback.print_exc()
                    return False

            # Monitor for changes
            last_data = None
            start_time = time.time()

            print(f"Starting monitoring loop for {duration} seconds with {interval} second interval")
            print("Press Ctrl+C to stop monitoring")
            try:
                while time.time() - start_time < duration:
                    # Lock mutex
                    try:
                        if lock_mutex(mutex_handle, 100):  # Short timeout to avoid blocking
                            # Read data
                            try:
                                data = read_from_shared_memory(address, size)

                                # Check for changes
                                if data != last_data:
                                    print(f"Data changed: '{data}'")
                                    last_data = data

                                # Unlock mutex
                                unlock_mutex(mutex_handle)
                            except Exception as e:
                                print(f"ERROR: Failed to read from shared memory: {e}")
                                print("This could mean the shared memory was closed by another process.")
                                unlock_mutex(mutex_handle)
                                import traceback
                                traceback.print_exc()

                                # Ask user if they want to retry
                                try:
                                    response = input("Would you like to retry? (y/n): ").strip().lower()
                                    if response == 'y' or response == 'yes':
                                        print("Cleaning up resources and retrying...")
                                        unmap_shared_memory(address)
                                        close_handle(shm_handle)
                                        close_handle(mutex_handle)
                                        return monitor_shared_memory(name, size, interval, duration, max_retries, retry_interval)
                                    else:
                                        print("Exiting monitoring loop.")
                                        break
                                except KeyboardInterrupt:
                                    print("\nOperation cancelled by user.")
                                    break
                    except Exception as e:
                        print(f"WARNING: Error during mutex lock: {e}")
                        import traceback
                        traceback.print_exc()

                    time.sleep(interval)
            except KeyboardInterrupt:
                print("\nMonitoring interrupted by user")
            except Exception as e:
                print(f"ERROR: Exception during monitoring: {e}")
                import traceback
                traceback.print_exc()

            # Cleanup
            try:
                unmap_shared_memory(address)
                close_handle(shm_handle)
                close_handle(mutex_handle)
                print("Resources cleaned up successfully")
            except Exception as e:
                print(f"WARNING: Error during cleanup: {e}")
                import traceback
                traceback.print_exc()

            return True

        except Exception as e:
            if attempt < max_retries - 1:
                print(f"CRITICAL ERROR: Unexpected exception: {e}")
                print(f"Retrying in {retry_interval} seconds... (Attempt {attempt + 1}/{max_retries})")
                import traceback
                traceback.print_exc()
                time.sleep(retry_interval)
            else:
                print(f"CRITICAL ERROR: Unexpected exception after {max_retries} attempts: {e}")
                import traceback
                traceback.print_exc()
                return False

    print(f"Failed to monitor shared memory '{name}' after {max_retries} attempts")
    return False

def create_default_config():
    """Create a default configuration file with comments."""
    # Create a simple configuration without comment keys
    # Comments will be added when saving to file
    config = {
        "monitor": {
            "name": "TestSharedMemory",
            "size": None,  # Auto-detect size
            "interval": 1.0,
            "duration": 60.0,
            "retries": 5,
            "retry_interval": 2.0,
            "wait": True
        }
    }
    return config

def load_config(config_file):
    """Load configuration from a JSONC file (JSON with Comments)."""
    try:
        with open(config_file, 'r') as f:
            # Read the file content
            json_str = f.read()

            # Strip comments
            json_str = strip_comments(json_str)

            # Parse JSON
            config = json.loads(json_str)

        print(f"Loaded configuration from {config_file}")
        return config
    except Exception as e:
        print(f"Error loading configuration: {e}")
        print("Using default configuration")
        return create_default_config()

def save_config(config, config_file):
    """Save configuration to a JSONC file (JSON with Comments)."""
    try:
        # Create a string with comments
        config_str = "// Windows Shared Memory Test Configuration\n"
        config_str += "// This file is used by test_win_shared_memory.py to configure shared memory testing\n"
        config_str += "// Comments are supported in JSONC format (JSON with Comments)\n\n"

        # Add the JSON content
        with open(config_file, 'w') as f:
            f.write(config_str)
            json.dump(config, f, indent=4)

        print(f"Saved configuration to {config_file}")
        return True
    except Exception as e:
        print(f"Error saving configuration: {e}")
        return False

def print_examples():
    """Print usage examples."""
    examples = """
Examples:
---------
# Create a default configuration file (recommended first step)
python test_win_shared_memory.py --create-config

# Monitor shared memory using the configuration file (recommended approach)
python test_win_shared_memory.py --mode monitor --config shared_memory_config.jsonc

# Monitor shared memory with command line parameters
python test_win_shared_memory.py --mode monitor --name TestSharedMemory --duration 60 --wait

# Save current settings to configuration file
python test_win_shared_memory.py --mode monitor --name MySharedMemory --wait --save-config

# Other available modes (see configuration file for more options)
python test_win_shared_memory.py --mode read --name TestSharedMemory
python test_win_shared_memory.py --mode create --name TestSharedMemory --size 1024 --data "Hello from Python!"
"""
    print(examples)

def main():
    """Main function."""
    parser = argparse.ArgumentParser(
        description='Windows Shared Memory Test',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""
        Examples:
        ---------
        # Create a default configuration file (recommended first step)
        python test_win_shared_memory.py --create-config

        # Monitor shared memory using the configuration file (recommended approach)
        python test_win_shared_memory.py --mode monitor --config shared_memory_config.jsonc

        # Monitor shared memory with command line parameters
        python test_win_shared_memory.py --mode monitor --name TestSharedMemory --duration 60 --wait

        # For more examples, run:
        python test_win_shared_memory.py --examples
        """)
    )
    parser.add_argument('--mode', choices=['create', 'read', 'monitor'],
                        help='Operation mode')
    parser.add_argument('--name',
                        help='Name of the shared memory segment')
    parser.add_argument('--size', type=int,
                        help='Size of the shared memory segment in bytes (required for create mode)')
    parser.add_argument('--data',
                        help='Data to write to shared memory (create mode only)')
    parser.add_argument('--interval', type=float,
                        help='Monitoring interval in seconds (monitor mode only)')
    parser.add_argument('--duration', type=float,
                        help='Monitoring duration in seconds (monitor mode only)')
    parser.add_argument('--retries', type=int,
                        help='Number of retry attempts for operations')
    parser.add_argument('--retry-interval', type=float,
                        help='Interval between retry attempts in seconds')
    parser.add_argument('--wait', action='store_true',
                        help='Wait and retry indefinitely until successful')
    parser.add_argument('--config', default='shared_memory_config.jsonc',
                        help='Path to configuration file (JSONC format with comments)')
    parser.add_argument('--save-config', action='store_true',
                        help='Save current configuration to file')
    parser.add_argument('--create-config', action='store_true',
                        help='Create a default configuration file')
    parser.add_argument('--examples', action='store_true',
                        help='Show usage examples')

    args = parser.parse_args()

    # Show examples if requested
    if args.examples:
        print_examples()
        return 0

    # Create default configuration if requested
    if args.create_config:
        config = create_default_config()
        if save_config(config, args.config):
            print(f"Created default configuration file: {args.config}")
            print("You can edit this file and use it with --config")
            return 0
        return 1

    # Load configuration from file
    config = load_config(args.config) if os.path.exists(args.config) else create_default_config()

    # Override configuration with command line arguments
    mode = args.mode
    if not mode:
        # Default to monitor mode if not specified
        print("No mode specified. Using default mode: monitor")
        mode = "monitor"

    # Get configuration for the specified mode
    # Filter out comment keys (keys starting with "//")
    filtered_config = {k: v for k, v in config.items() if not k.startswith("//")}
    mode_config = filtered_config.get(mode, {})

    # Filter out comment keys from mode_config
    filtered_mode_config = {k: v for k, v in mode_config.items() if not k.startswith("//")}

    # Get values from config file first, then override with command line arguments only if specified
    name = filtered_mode_config.get('name', 'TestSharedMemory')
    if args.name:
        name = args.name

    size = filtered_mode_config.get('size')
    if args.size is not None:
        size = args.size

    data = filtered_mode_config.get('data', 'Hello from Python!')
    if args.data:
        data = args.data

    interval = filtered_mode_config.get('interval', 1.0)
    if args.interval:
        interval = args.interval

    duration = filtered_mode_config.get('duration', 60.0)
    if args.duration:
        duration = args.duration

    retries = filtered_mode_config.get('retries', 5)
    if args.retries:
        retries = args.retries

    retry_interval = filtered_mode_config.get('retry_interval', 2.0)
    if args.retry_interval:
        retry_interval = args.retry_interval

    wait = filtered_mode_config.get('wait', False)
    if args.wait:
        wait = args.wait

    print(f"Using configuration: mode={mode}, name={name}, wait={wait}")

    # Update configuration with current values
    mode_config.update({
        'name': name,
        'size': size,
        'data': data,
        'interval': interval,
        'duration': duration,
        'retries': retries,
        'retry_interval': retry_interval,
        'wait': wait
    })
    config[mode] = mode_config

    # Save configuration if requested
    if args.save_config:
        if save_config(config, args.config):
            print(f"Configuration saved to {args.config}")
        else:
            print(f"Failed to save configuration to {args.config}")

    # If --wait is specified, set retries to a very large number
    max_retries = 1000 if wait else retries

    # Execute the requested operation
    if mode == 'create':
        if size is None:
            print("Error: Size must be specified for create mode")
            return 1
        if create_and_write(name, size, data):
            return 0
        return 1

    elif mode == 'read':
        data = open_and_read(name, size, max_retries, retry_interval)
        if data is not None:
            return 0
        return 1

    elif mode == 'monitor':
        if monitor_shared_memory(name, size, interval, duration, max_retries, retry_interval):
            return 0
        return 1

if __name__ == '__main__':
    sys.exit(main())
