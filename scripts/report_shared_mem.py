import mmap
import os
import time
import struct
from ctypes import windll, c_void_p, sizeof, byref, create_string_buffer, WinError
from ctypes import c_ulong, c_char_p, POINTER, Structure, c_bool, c_uint32, pointer, c_size_t, cast
from ctypes import c_int, c_long, c_ulonglong, CFUNCTYPE

# Define constants and structures for NtQuerySection
SectionBasicInformation = 0

class LARGE_INTEGER(Structure):
    _fields_ = [
        ("QuadPart", c_ulonglong)
    ]

class SECTION_BASIC_INFORMATION(Structure):
    _fields_ = [
        ("BaseAddress", c_void_p),
        ("AllocationAttributes", c_ulong),
        ("MaximumSize", LARGE_INTEGER)
    ]

def get_section_size(handle, verbose=False):
    """
    Get the size of a section object using NtQuerySection.
    
    Args:
        handle: Handle to the section object
        verbose: Whether to display detailed information
        
    Returns:
        Size of the section in bytes, or None if the query fails
    """
    # Load ntdll.dll
    ntdll = windll.ntdll
    
    # Define the function prototype
    NtQuerySection = ntdll.NtQuerySection
    NtQuerySection.argtypes = [c_void_p, c_int, c_void_p, c_ulong, POINTER(c_ulong)]
    NtQuerySection.restype = c_long
    
    # Create the output buffer
    section_info = SECTION_BASIC_INFORMATION()
    return_length = c_ulong(0)
    
    # Call NtQuerySection
    status = NtQuerySection(
        handle,
        SectionBasicInformation,
        byref(section_info),
        sizeof(SECTION_BASIC_INFORMATION),
        byref(return_length)
    )
    
    if status != 0:
        # Convert to unsigned for proper hex representation
        unsigned_status = status & 0xFFFFFFFF
        print(f"NtQuerySection failed with status: 0x{unsigned_status:08X}")
        
        # Special handling for known error codes
        if unsigned_status == 0x3FFFFFDE:
            print("This appears to be a STATUS_INFO_LENGTH_MISMATCH or similar error.")
            print("Trying alternative approach with different buffer size...")
            
            # Try with a larger buffer
            buffer_size = return_length.value if return_length.value > 0 else sizeof(SECTION_BASIC_INFORMATION) * 2
            buffer = create_string_buffer(buffer_size)
            
            status = NtQuerySection(
                handle,
                SectionBasicInformation,
                buffer,
                buffer_size,
                byref(return_length)
            )
            
            if status == 0:
                print(f"Alternative approach succeeded! Return length: {return_length.value}")
                # We would need to parse the buffer manually here
                # For now, just use VirtualQuery as fallback
            else:
                print(f"Alternative approach also failed with status: 0x{status & 0xFFFFFFFF:08X}")
        
        return None
    
    # Extract the size from the result
    size = section_info.MaximumSize.QuadPart
    
    if verbose:
        print("\n=== Section Information ===")
        print(f"Maximum size: {size} bytes")
        attrs_str = get_allocation_attributes_string(section_info.AllocationAttributes)
        print(f"Allocation attributes: {attrs_str}")
        if section_info.BaseAddress:
            print(f"Base address: 0x{int(cast(section_info.BaseAddress, c_void_p).value):X}")
    else:
        print(f"Section allocation attributes: 0x{section_info.AllocationAttributes:X}")
    
    return size

def get_memory_info(map_view):
    """
    Get information about the mapped memory region using VirtualQuery.
    
    Args:
        map_view: Pointer to the mapped view of file
        
    Returns:
        Dictionary with memory information or None if query fails
    """
    # Define memory basic information structure
    class MEMORY_BASIC_INFORMATION(Structure):
        _fields_ = [
            ("BaseAddress", c_void_p),
            ("AllocationBase", c_void_p),
            ("AllocationProtect", c_ulong),
            ("RegionSize", c_size_t),
            ("State", c_ulong),
            ("Protect", c_ulong),
            ("Type", c_ulong)
        ]
    
    mbi = MEMORY_BASIC_INFORMATION()
    result = windll.kernel32.VirtualQuery(
        map_view,
        byref(mbi),
        sizeof(MEMORY_BASIC_INFORMATION)
    )
    
    if result == 0:
        error_code = windll.kernel32.GetLastError()
        print(f"Failed to query memory information: {WinError(error_code)}")
        return None
    
    return {
        "base_address": mbi.BaseAddress,
        "allocation_base": mbi.AllocationBase,
        "allocation_protect": mbi.AllocationProtect,
        "region_size": mbi.RegionSize,
        "state": mbi.State,
        "protect": mbi.Protect,
        "type": mbi.Type
    }

def get_memory_state_string(state):
    """Convert memory state value to string description"""
    MEM_COMMIT = 0x1000
    MEM_RESERVE = 0x2000
    MEM_FREE = 0x10000
    
    if state == MEM_COMMIT:
        return "MEM_COMMIT"
    elif state == MEM_RESERVE:
        return "MEM_RESERVE"
    elif state == MEM_FREE:
        return "MEM_FREE"
    else:
        return f"Unknown (0x{state:X})"

def get_memory_type_string(type_val):
    """Convert memory type value to string description"""
    MEM_PRIVATE = 0x20000
    MEM_MAPPED = 0x40000
    MEM_IMAGE = 0x1000000
    
    if type_val == MEM_PRIVATE:
        return "MEM_PRIVATE"
    elif type_val == MEM_MAPPED:
        return "MEM_MAPPED"
    elif type_val == MEM_IMAGE:
        return "MEM_IMAGE"
    else:
        return f"Unknown (0x{type_val:X})"

def get_memory_protection_string(protect):
    """Convert memory protection value to string description"""
    PAGE_NOACCESS = 0x01
    PAGE_READONLY = 0x02
    PAGE_READWRITE = 0x04
    PAGE_WRITECOPY = 0x08
    PAGE_EXECUTE = 0x10
    PAGE_EXECUTE_READ = 0x20
    PAGE_EXECUTE_READWRITE = 0x40
    PAGE_EXECUTE_WRITECOPY = 0x80
    PAGE_GUARD = 0x100
    PAGE_NOCACHE = 0x200
    PAGE_WRITECOMBINE = 0x400
    
    result = []
    
    # Base protection
    if protect & PAGE_NOACCESS:
        result.append("PAGE_NOACCESS")
    if protect & PAGE_READONLY:
        result.append("PAGE_READONLY")
    if protect & PAGE_READWRITE:
        result.append("PAGE_READWRITE")
    if protect & PAGE_WRITECOPY:
        result.append("PAGE_WRITECOPY")
    if protect & PAGE_EXECUTE:
        result.append("PAGE_EXECUTE")
    if protect & PAGE_EXECUTE_READ:
        result.append("PAGE_EXECUTE_READ")
    if protect & PAGE_EXECUTE_READWRITE:
        result.append("PAGE_EXECUTE_READWRITE")
    if protect & PAGE_EXECUTE_WRITECOPY:
        result.append("PAGE_EXECUTE_WRITECOPY")
    
    # Modifiers
    if protect & PAGE_GUARD:
        result.append("PAGE_GUARD")
    if protect & PAGE_NOCACHE:
        result.append("PAGE_NOCACHE")
    if protect & PAGE_WRITECOMBINE:
        result.append("PAGE_WRITECOMBINE")
    
    if not result:
        return f"Unknown (0x{protect:X})"
    
    return " | ".join(result)

def get_allocation_attributes_string(attrs):
    """Convert allocation attributes to string description"""
    result = []
    
    # Common allocation attributes
    if attrs & 0x00000001:  # SEC_COMMIT
        result.append("SEC_COMMIT")
    if attrs & 0x00800000:  # SEC_RESERVE
        result.append("SEC_RESERVE")
    if attrs & 0x08000000:  # SEC_NOCACHE
        result.append("SEC_NOCACHE")
    if attrs & 0x10000000:  # SEC_IMAGE
        result.append("SEC_IMAGE")
    if attrs & 0x01000000:  # SEC_LARGE_PAGES
        result.append("SEC_LARGE_PAGES")
    if attrs & 0x00400000:  # SEC_BASED
        result.append("SEC_BASED")
    
    if not result:
        return f"0x{attrs:X}"
    
    return " | ".join(result)

def get_process_info():
    """Get information about the current process"""
    # Get process ID
    pid = os.getpid()
    
    # Get process handle
    process_handle = windll.kernel32.GetCurrentProcess()
    
    # Get process memory info
    class PROCESS_MEMORY_COUNTERS(Structure):
        _fields_ = [
            ("cb", c_ulong),
            ("PageFaultCount", c_ulong),
            ("PeakWorkingSetSize", c_size_t),
            ("WorkingSetSize", c_size_t),
            ("QuotaPeakPagedPoolUsage", c_size_t),
            ("QuotaPagedPoolUsage", c_size_t),
            ("QuotaPeakNonPagedPoolUsage", c_size_t),
            ("QuotaNonPagedPoolUsage", c_size_t),
            ("PagefileUsage", c_size_t),
            ("PeakPagefileUsage", c_size_t)
        ]
    
    pmc = PROCESS_MEMORY_COUNTERS()
    pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS)
    
    if windll.psapi.GetProcessMemoryInfo(process_handle, byref(pmc), sizeof(pmc)):
        return {
            "pid": pid,
            "working_set_size": pmc.WorkingSetSize,
            "peak_working_set_size": pmc.PeakWorkingSetSize,
            "pagefile_usage": pmc.PagefileUsage,
            "peak_pagefile_usage": pmc.PeakPagefileUsage
        }
    else:
        return {"pid": pid}

def open_shared_memory(name="MySharedMemory", read_only=False, verbose=False):
    """
    Open a Windows shared memory object by name.
    
    Args:
        name: Name of the shared memory object
        read_only: Whether to open in read-only mode (default is False)
        verbose: Whether to display detailed information (default is False)
    
    Returns:
        A tuple containing (memory map object, handle, map_view) if successful, (None, None, None) otherwise
    """
    # Constants for Windows API
    FILE_MAP_READ = 0x0004
    FILE_MAP_WRITE = 0x0002
    FILE_MAP_ALL_ACCESS = 0x000F001F  # Full access
    
    # Determine access mode based on read_only flag
    access_mode = FILE_MAP_READ if read_only else FILE_MAP_ALL_ACCESS
    
    print(f"Attempting to open shared memory '{name}' in {'read-only' if read_only else 'read-write'} mode")
    
    # Get handle to the shared memory with appropriate access
    handle = windll.kernel32.OpenFileMappingA(
        access_mode,          # Access rights based on read_only flag
        False,                # Don't inherit handle
        name.encode('ascii')  # Name of mapping object
    )
    
    if handle == 0:
        error_code = windll.kernel32.GetLastError()
        print(f"Failed to open shared memory '{name}': {WinError(error_code)}")
        return None, None, None
    
    # Try to get section size using NtQuerySection
    actual_size = get_section_size(handle, verbose)
    
    # If NtQuerySection fails, use a default size
    if actual_size is None:
        print("NtQuerySection failed, using default size")
        actual_size = 1024 * 1024  # 1MB default size
    
    # Map view of the file with the size
    map_view = windll.kernel32.MapViewOfFile(
        handle,              # Handle to mapping object
        access_mode,         # Access mode based on read_only flag
        0,                   # High-order DWORD of offset
        0,                   # Low-order DWORD of offset
        actual_size          # Number of bytes to map
    )
    
    if map_view == 0:
        error_code = windll.kernel32.GetLastError()
        print(f"Failed to map view of file: {WinError(error_code)}")
        windll.kernel32.CloseHandle(handle)
        return None, None, None
    
    # Get memory information for verbose output only
    if verbose:
        memory_info = get_memory_info(map_view)
        if memory_info:
            print("\n=== Memory Information ===")
            print(f"Base address: 0x{int(cast(memory_info['base_address'], c_void_p).value):X}")
            # Note: Region size from VirtualQuery may not accurately reflect the actual shared memory size
            print(f"State: {get_memory_state_string(memory_info['state'])}")
            print(f"Type: {get_memory_type_string(memory_info['type'])}")
            print(f"Protection: {get_memory_protection_string(memory_info['protect'])}")
            print(f"Allocation protection: {get_memory_protection_string(memory_info['allocation_protect'])}")
    
    # Create a memory map from the pointer
    try:
        # Create a memory map using the shared memory
        if read_only:
            # Create a read-only memory map
            shared_mem = mmap.mmap(-1, actual_size, access=mmap.ACCESS_READ, tagname=name)
        else:
            # Create a read-write memory map
            shared_mem = mmap.mmap(-1, actual_size, access=mmap.ACCESS_WRITE, tagname=name)
        
        print(f"Successfully opened shared memory '{name}' with size {actual_size} bytes")
        return shared_mem, handle, map_view
    except Exception as e:
        print(f"Error creating memory map: {e}")
        windll.kernel32.UnmapViewOfFile(map_view)
        windll.kernel32.CloseHandle(handle)
        return None, None, None

def main():
    # Parse command line arguments
    import argparse
    parser = argparse.ArgumentParser(description='Open and interact with Windows shared memory.')
    parser.add_argument('--name', default="MySharedMemory", help='Name of the shared memory object')
    parser.add_argument('--read-only', action='store_true', help='Open in read-only mode (default is read-write)')
    parser.add_argument('--write-data', action='store_true', help='Write data to shared memory (default is read-only)')
    parser.add_argument('--message', default="Hello from Python!", help='Message to write if write-data is enabled')
    parser.add_argument('--verbose', '-v', action='store_true', help='Display detailed memory information')
    args = parser.parse_args()
    
    # Show process information if verbose mode is enabled
    if args.verbose:
        process_info = get_process_info()
        print("\n=== Process Information ===")
        print(f"Process ID: {process_info.get('pid')}")
        if 'working_set_size' in process_info:
            print(f"Working set size: {process_info['working_set_size'] / 1024:.2f} KB")
            print(f"Peak working set size: {process_info['peak_working_set_size'] / 1024:.2f} KB")
            print(f"Pagefile usage: {process_info['pagefile_usage'] / 1024:.2f} KB")
            print(f"Peak pagefile usage: {process_info['peak_pagefile_usage'] / 1024:.2f} KB")
    
    # Try to open the shared memory
    read_only = args.read_only
    shared_mem, handle, map_view = open_shared_memory(args.name, read_only=read_only, verbose=args.verbose)
    
    if shared_mem:
        try:
            # Read data from shared memory
            shared_mem.seek(0)
            data = shared_mem.read(100)  # Read up to 100 bytes
            try:
                # Try to decode as UTF-8
                decoded_data = data.decode('utf-8', errors='replace').rstrip('\x00')
                print(f"\nRead from shared memory: {decoded_data}")
            except UnicodeDecodeError:
                # If decoding fails, show as bytes
                print(f"\nRead from shared memory (binary): {data}")
            
            # Write to shared memory if write-data flag is set and not in read-only mode
            if args.write_data:
                if read_only:
                    print("Cannot write data in read-only mode.")
                else:
                    shared_mem.seek(0)
                    message = args.message.encode('utf-8')
                    shared_mem.write(message)
                    shared_mem.flush()
                    print(f"Wrote to shared memory: {args.message}")
            else:
                print("Write mode not enabled. Use --write-data to write to shared memory.")
            
            # Keep the script running to maintain access
            print("\nPress Ctrl+C to exit...")
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("Exiting...")
        finally:
            shared_mem.close()
            if map_view:
                windll.kernel32.UnmapViewOfFile(map_view)
            if handle:
                windll.kernel32.CloseHandle(handle)

if __name__ == "__main__":
    main()
