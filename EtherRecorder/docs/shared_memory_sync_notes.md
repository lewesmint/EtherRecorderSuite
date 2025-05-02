# UDP Shared Memory Synchronization Notes

## Overview
This document outlines the design and implementation details for the UDP shared memory synchronization feature in EtherRecorder. This feature allows shared memory blocks to be synchronized across multiple PCs using UDP communication.

## Shared Memory Concept
- Shared memory allows multiple processes on the same machine to access the same memory region
- Windows-only implementation using `CreateFileMapping` and `MapViewOfFile` APIs
- Changes to shared memory are monitored and transmitted via UDP to other machines

## INI File Configuration
The configuration for shared memory synchronization will be stored in the `config.ini` file under a new `[shared_memory_sync]` section:

```ini
[shared_memory_sync]
; Format: block_name.property=value
block1.name=SharedBlock1       ; Name of the shared memory block
block1.hostname=192.168.1.100
block1.access=read             ; read, write, or readwrite
block1.size=1024               ; size in bytes
block1.port=5000               ; UDP port for communication
block1.create=true             ; Whether to create the shared memory if it doesn't exist

block2.name=SharedBlock2       ; Name of the shared memory block
block2.hostname=192.168.1.101
block2.access=write
block2.size=2048
block2.port=5001               ; UDP port for communication
block2.create=false            ; Only open existing shared memory, don't create
```

### Configuration Properties
- `block_name.name`: Name of the shared memory block (used for Windows named shared memory)
- `block_name.hostname`: IP address or hostname of the remote PC to sync with
- `block_name.access`: Access mode (read, write, or readwrite)
  - `read`: Monitor local shared memory for changes and send updates to remote host
  - `write`: Listen for updates from remote host and apply to local shared memory
  - `readwrite`: Both read and write operations
- `block_name.size`: Size of the shared memory block in bytes
- `block_name.port`: UDP port number for communication with the remote host
- `block_name.create`: Whether to create the shared memory if it doesn't exist (true/false)
  - `true`: Create the shared memory block if it doesn't exist
  - `false`: Only open existing shared memory, fail if it doesn't exist
- `block_name.update_interval_ms`: (Optional) Interval in milliseconds for checking memory changes (default: 100)
- `block_name.retry_count`: (Optional) Number of retries for failed transmissions (default: 3)

## Synchronization Logic

### Read Mode
- Monitor local shared memory for changes
- Detect changes efficiently using one of these methods:
  - Memory comparison: Compare current memory state with previous snapshot
  - Checksum: Calculate checksums for memory regions and compare with previous values
  - Dirty flags: Maintain a bitmap of modified memory regions
  - Timestamp-based: Track last modification time for memory regions
- Send only changed portions to minimize network traffic
- Transmit changes to the specified remote host via UDP

### Write Mode
- Listen for updates from remote host
- Receive updates via UDP
- Apply changes to local shared memory

### Read-Write Mode
- Combination of both read and write modes
- Need to handle potential conflicts or race conditions

## UDP Communication Protocol

### Message Types
1. **INIT_SYNC (0x01)**: Initial synchronization request/response
2. **MEM_UPDATE (0x02)**: Memory update message
3. **ACK (0x03)**: Acknowledgment of received message
4. **FULL_SYNC (0x04)**: Full memory state synchronization
5. **HEARTBEAT (0x05)**: Connection keepalive message

### Message Format
```
+----------------+----------------+----------------+----------------+
| Message Type   | Sequence Num   | Block Name Len | Block Name     |
| (1 byte)       | (2 bytes)      | (1 byte)       | (variable)     |
+----------------+----------------+----------------+----------------+
| Offset         | Data Length    | Data           |
| (4 bytes)      | (4 bytes)      | (variable)     |
+----------------+----------------+----------------+
```

### Protocol Considerations
- UDP is unreliable, so we need to handle packet loss
- Implement sequence numbers for detecting missed updates
- Perform periodic full state synchronization (every N seconds or after X missed updates)
- Implement acknowledgments for critical updates
- Use heartbeat messages to detect connection status

## Windows-Specific Implementation

### Shared Memory
- Use Windows memory-mapped files for shared memory
- Create named shared memory segments using `CreateFileMapping`
- Map views of shared memory into process address space using `MapViewOfFile`
- Support both creating new shared memory and opening existing segments
- When opening existing shared memory, the size is determined by the creator
- Use named mutexes for cross-process synchronization of shared memory access
- Handle permissions and security descriptors appropriately
- Implement proper cleanup with `UnmapViewOfFile` and `CloseHandle`

### Implementation Findings
- **Shared Memory Naming**: Keep shared memory names simple. Using the "Global\" prefix requires elevated permissions and may fail in standard user contexts.
- **Handle Lifetime**: Shared memory objects are automatically destroyed when all handles to them are closed. To maintain the shared memory between operations, keep at least one handle open.
- **Size Determination**: When opening an existing shared memory segment, the size must be queried using `VirtualQuery` as it's determined by the creator.
- **Error Handling**: Windows error codes provide valuable diagnostic information. Always check both the platform error code and the Windows error code (via `GetLastError()`).
- **Synchronization**: Named mutexes work well for cross-process synchronization, but ensure mutex names are unique to avoid conflicts.
- **Memory Mapping**: Always unmap views of shared memory before closing handles to avoid resource leaks.
- **Testing Strategy**: A dual approach using both C tests (linked with the library) and independent Python tests provides comprehensive verification.

### Security Considerations
- Consider who can access the shared memory
- Implement appropriate security descriptors

### Performance Considerations
- Minimize memory copying
- Implement efficient change detection
- Optimize UDP packet size for network performance

## Thread Management
- Dedicated thread for monitoring shared memory changes
- Dedicated thread for receiving UDP updates
- Thread synchronization to avoid race conditions

## Error Handling
- Handle network errors gracefully
  - Retry failed transmissions based on configuration
  - Log network errors with detailed diagnostics
  - Implement exponential backoff for repeated failures
- Handle shared memory access errors
  - Detect and log access violations
  - Attempt to recreate shared memory segments if corrupted
  - Notify application of critical shared memory failures
- Implement recovery mechanisms
  - Automatic reconnection after network failures
  - Periodic full synchronization to recover from desynchronized states
  - Fallback to application-defined default values when synchronization fails
- Log detailed error information
  - Include timestamps, error codes, and context information
  - Log both to file and console for critical errors
- Graceful degradation when synchronization fails
  - Continue operation with local-only memory access
  - Notify application of synchronization status changes
  - Provide API for applications to check sync status

## Implementation Plan
1. Create Windows shared memory implementation
   - Implement functions for creating, opening, and mapping shared memory
   - Create helper functions for reading and writing to shared memory
   - Add error handling and resource cleanup

2. Test shared memory implementation
   - Create C test program that links statically with PlatformLayer
   - Create Python test scripts for independent verification
   - Verify cross-process communication works correctly

3. Add shared memory synchronization configuration to INI file
   - Define the [shared_memory_sync] section format
   - Implement parsing of shared memory configuration
   - Add validation for configuration parameters

4. Create shared memory synchronization manager
   - Implement the manager to handle multiple shared memory blocks
   - Create change detection mechanism
   - Implement synchronization logic for read/write modes

5. Integrate with existing UDP socket implementation
   - Create message format for shared memory updates
   - Implement message serialization/deserialization
   - Add UDP send/receive functionality for memory updates

6. Add thread for shared memory synchronization
   - Create monitoring thread for detecting changes
   - Implement UDP listener thread for receiving updates
   - Add thread synchronization mechanisms

7. Update main application to initialize shared memory sync
   - Add initialization code to main.c
   - Register shared memory sync threads with thread registry
   - Implement proper shutdown handling

## Limitations
- Windows-only feature
  - Implementation relies on Windows-specific APIs
  - Not compatible with other operating systems
- UDP reliability limitations
  - Packet loss may occur in congested networks
  - No built-in delivery guarantees
  - May require application-level reliability mechanisms
- Network performance considerations
  - High update frequency can consume significant bandwidth
  - Network latency affects synchronization responsiveness
  - Large memory blocks may require fragmentation
- Potential security concerns with shared memory access
  - Shared memory accessible to all processes with appropriate permissions
  - No built-in encryption for network transmission
  - Consider security implications in sensitive applications

## Future Enhancements
- Implement compression for large memory blocks
- Add support for selective synchronization of memory regions
- Create monitoring and diagnostic tools
- Add configuration UI for easier setup
- Add support for multiple synchronization strategies
