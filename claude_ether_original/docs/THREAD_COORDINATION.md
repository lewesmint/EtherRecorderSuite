# Thread Coordination Design

## Overview
This document describes the thread coordination system used for managing socket-based communication threads in the Claude Ether project.

## Core Components

### Thread Groups
Thread groups provide coordinated lifecycle management for sets of related threads, particularly those handling socket communications. A typical group consists of:
- Send thread (outbound data)
- Receive thread (inbound data)
- Manager thread (connection supervision)

### Key Structures

#### CommsThreadGroup
Manages the lifecycle of communication-related threads:
- Maintains socket state
- Tracks connection status
- Coordinates thread shutdown
- Manages message routing between threads

#### ThreadRegistry
Global thread management system that:
- Tracks all active threads and their group associations
- Handles coordinated shutdown of thread groups
- Manages thread resource cleanup
- Provides thread state querying capabilities

## Communication Flow

1. Connection Establishment
   - Manager thread accepts new connection
   - Creates CommsThreadGroup
   - Spawns send/receive threads
   - Registers threads with shared group ID

2. Normal Operation
   - Receive thread processes incoming data
   - Send thread handles outgoing data
   - All threads monitor connection status
   - Group ID enables message routing between threads

3. Shutdown Sequence
   - Any thread can trigger group shutdown
   - Registry notifies all group members
   - Coordinated cleanup of shared resources
   - Socket closure synchronized across threads

## Resource Management

### Socket Lifecycle
- Socket owned by CommsThreadGroup
- All group threads share socket
- Last thread in group responsible for cleanup
- Connection status tracked via atomic flag

### Message Queues
- Per-thread queues for async communication
- Thread registry manages queue allocation
- Group ID used for message routing
- Queue cleanup tied to thread lifecycle

## Error Handling

### Connection Loss
1. Detecting thread sets connection_closed flag
2. Other threads in group observe flag
3. Group initiates coordinated shutdown
4. Registry ensures cleanup completion

### Thread Failure
1. Failed thread marks group for termination
2. Registry initiates group shutdown
3. Surviving threads cleanup resources
4. Manager thread notified for potential reconnection

## Implementation Guidelines

### Thread Creation
```c
// Example of proper thread group setup
CommsThreadGroup group;
comms_thread_group_init(&group, "CLIENT_CONN", socket, &connection_flag);

// Create and register threads with shared group ID
AppThread_T send_thread = {
    .label = "send_thread",
    .group_id = group.group_id,  // Share group ID
    // ... other initialization
};
```

### Resource Cleanup
```c
// Proper cleanup sequence
comms_thread_group_close(&group);        // Mark for shutdown
comms_thread_group_wait(&group, 1000);   // Wait for threads
comms_thread_group_cleanup(&group);      // Clean resources
```

## Future Considerations

1. Dynamic Thread Addition
   - Support adding threads to existing groups
   - Handle dynamic resource allocation
   - Maintain thread coordination

2. Group Hierarchy
   - Support nested thread groups
   - Parent-child relationship handling
   - Cascading shutdown mechanisms

3. State Machine Integration
   - Formal state transitions
   - Better error recovery
   - Enhanced debugging

## Related Files
- `comm_threads.h/c`: Communication thread implementation
- `thread_registry.h/c`: Thread registration and management
- `thread_group.h/c`: Thread group primitives
- `server_manager.c`: Usage examples in server context