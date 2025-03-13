# Thread Group Implementation Proposal

## Overview
Thread groups will be implemented using a master thread handle as the group identifier. Each thread in a group references its master thread's handle as its group ID, creating a natural hierarchy without additional data structures.

## Design

### Core Concept
- Master thread is the group leader
- Group members store master's thread handle as their group ID
- No explicit group structure needed
- Natural hierarchy through handle references

### Key Benefits
1. Simple implementation
2. No additional synchronization needed
3. Natural lifecycle management
4. Easy group membership checks
5. Efficient thread relationship tracking

### Proposed API

#### Thread Creation
```c
ThreadRegistryError thread_group_create_thread(
    const ThreadConfig* master_thread,
    ThreadConfig* thread_config
);
```

#### Group Operations
```c
bool thread_group_contains(
    PlatformThreadHandle master_handle,
    PlatformThreadHandle thread_handle
);

ThreadConfig** thread_group_get_threads(
    PlatformThreadHandle master_handle,
    size_t* thread_count
);

ThreadRegistryError thread_group_terminate(
    PlatformThreadHandle master_handle
);
```

### Usage Example
```c
// Create master thread
ThreadConfig master = {
    .label = "MASTER",
    // ... other config
};
thread_registry_register(&master, master_handle, false, 0);  // No group ID for master

// Create worker thread in master's group
ThreadConfig worker = {
    .label = "WORKER",
    // ... other config
};
thread_group_create_thread(&master, &worker);
```

## Implementation Notes

### Thread Registry Integration
- Thread registry stores group ID (master's handle)
- Registry queries can filter by group ID
- Group operations work through registry

### Lifecycle Management
1. Master thread creation
   - Registered with no group ID (0)
   - Handle becomes group ID for members

2. Member thread creation
   - Created with master's handle as group ID
   - Automatically associated with group

3. Group termination
   - Can terminate all threads by master's handle
   - Natural cleanup through registry

### Future Considerations
1. Group-wide operations
   - Message broadcasting
   - Synchronized actions
   - Resource sharing

2. Nested groups
   - Sub-master threads
   - Hierarchy traversal
   - Multi-level operations

3. Group statistics
   - Member count tracking
   - Resource usage
   - Performance metrics

## Implementation Priority
- [ ] Add group ID field to thread registry
- [ ] Implement basic group operations
- [ ] Add group-aware thread creation
- [ ] Implement group termination
- [ ] Add group query functions