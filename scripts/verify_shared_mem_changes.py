#!/usr/bin/env python3
"""
Verify changes in Windows shared memory.
This script monitors a shared memory block and reports changes over time.
"""

import time
import argparse
import mmap
import sys
from ctypes import *
from ctypes.wintypes import *
import binascii

# Import functions from report_shared_mem.py
from report_shared_mem import (
    open_shared_memory, 
    get_memory_info, 
    get_memory_protection_string,
    get_memory_state_string,
    get_memory_type_string
)

def monitor_shared_memory(name, interval=1.0, max_time=60, read_only=True, verbose=False, offset=0, length=None):
    """
    Monitor a shared memory block for changes.
    
    Args:
        name: Name of the shared memory object
        interval: Check interval in seconds
        max_time: Maximum monitoring time in seconds
        read_only: Whether to open in read-only mode
        verbose: Whether to display detailed information
        offset: Starting offset to monitor (bytes)
        length: Length to monitor (bytes, None for entire block)
    """
    print(f"Monitoring shared memory '{name}' for changes (interval: {interval}s, max time: {max_time}s)")
    
    # Open the shared memory
    shared_mem, handle, map_view = open_shared_memory(name, read_only=read_only, verbose=verbose)
    
    if not shared_mem:
        print(f"Failed to open shared memory '{name}'")
        return False
    
    try:
        # Get total size
        shared_mem.seek(0, 2)  # Seek to end
        total_size = shared_mem.tell()
        
        # Validate offset and length
        if offset >= total_size:
            print(f"Error: Offset {offset} is beyond the end of shared memory (size: {total_size})")
            return False
        
        if length is None:
            length = total_size - offset
        elif offset + length > total_size:
            print(f"Warning: Requested length extends beyond end of shared memory. Adjusting to {total_size - offset}")
            length = total_size - offset
        
        print(f"Monitoring region: offset={offset}, length={length}, total size={total_size}")
        
        # Take initial snapshot
        shared_mem.seek(offset)
        previous_data = shared_mem.read(length)
        previous_hash = binascii.crc32(previous_data)
        
        print(f"Initial snapshot taken: {len(previous_data)} bytes, CRC32: 0x{previous_hash:08X}")
        
        # Monitor for changes
        start_time = time.time()
        changes_detected = 0
        last_change_time = start_time
        
        while time.time() - start_time < max_time:
            time.sleep(interval)
            
            # Take new snapshot
            shared_mem.seek(offset)
            current_data = shared_mem.read(length)
            current_hash = binascii.crc32(current_data)
            
            # Check for changes
            if current_hash != previous_hash:
                changes_detected += 1
                current_time = time.time()
                time_since_last = current_time - last_change_time
                last_change_time = current_time
                
                print(f"\nChange #{changes_detected} detected at {time.strftime('%H:%M:%S')} ({time_since_last:.2f}s since last change):")
                print(f"  Previous CRC32: 0x{previous_hash:08X}")
                print(f"  Current CRC32:  0x{current_hash:08X}")
                
                # Find differences if data size is the same
                if len(current_data) == len(previous_data):
                    diff_count = 0
                    diff_positions = []
                    
                    for i, (prev_byte, curr_byte) in enumerate(zip(previous_data, current_data)):
                        if prev_byte != curr_byte:
                            diff_count += 1
                            if len(diff_positions) < 10:  # Limit to first 10 differences
                                diff_positions.append((i + offset, prev_byte, curr_byte))
                    
                    print(f"  {diff_count} bytes changed")
                    
                    if diff_positions:
                        print("  First few changes:")
                        for pos, old, new in diff_positions:
                            print(f"    Offset 0x{pos:08X}: 0x{old:02X} -> 0x{new:02X}")
                            
                            # Try to interpret as different data types
                            if i >= 4:  # Need at least 4 bytes for int32
                                try:
                                    old_int = int.from_bytes(previous_data[i-4:i], byteorder='little')
                                    new_int = int.from_bytes(current_data[i-4:i], byteorder='little')
                                    print(f"      As int32: {old_int} -> {new_int}")
                                except:
                                    pass
                else:
                    print(f"  Data size changed: {len(previous_data)} -> {len(current_data)} bytes")
                
                # Update previous data
                previous_data = current_data
                previous_hash = current_hash
            else:
                sys.stdout.write(".")
                sys.stdout.flush()
        
        print(f"\n\nMonitoring completed. Detected {changes_detected} changes over {max_time} seconds.")
        return changes_detected > 0
        
    except KeyboardInterrupt:
        print("\nMonitoring interrupted by user.")
    finally:
        # Clean up
        shared_mem.close()
        if map_view:
            windll.kernel32.UnmapViewOfFile(map_view)
        if handle:
            windll.kernel32.CloseHandle(handle)
        print("Resources cleaned up.")

def main():
    parser = argparse.ArgumentParser(description='Monitor Windows shared memory for changes.')
    parser.add_argument('--name', default="Venus", help='Name of the shared memory object')
    parser.add_argument('--interval', type=float, default=0.1, help='Check interval in seconds')
    parser.add_argument('--time', type=int, default=60, help='Maximum monitoring time in seconds')
    parser.add_argument('--read-only', action='store_true', help='Open in read-only mode')
    parser.add_argument('--verbose', '-v', action='store_true', help='Display detailed information')
    parser.add_argument('--offset', type=int, default=0, help='Starting offset to monitor (bytes)')
    parser.add_argument('--length', type=int, default=None, help='Length to monitor (bytes, default: entire block)')
    args = parser.parse_args()
    
    monitor_shared_memory(
        args.name, 
        interval=args.interval, 
        max_time=args.time, 
        read_only=args.read_only,
        verbose=args.verbose,
        offset=args.offset,
        length=args.length
    )

if __name__ == "__main__":
    main()

