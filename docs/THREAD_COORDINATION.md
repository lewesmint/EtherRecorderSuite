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

#### Communication Context
```c
typedef struct CommsThreadGroup {
    const char* group_label;       // Group identifier
    atomic_bool active;            // Group activity status
    PlatformSocket socket;         // Shared socket
    atomic_bool* connection_flag;  // Connection status
    MessageQueue_T* send_queue;    // Queue for outbound messages
    MessageQueue_T* recv_queue;    // Queue for inbound messages
    const char* owner_label;       // Added owner label
} CommsContext;
```
