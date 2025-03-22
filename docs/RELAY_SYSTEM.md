# Relay System Design

## Primary Purpose
The Claude Ether relay system's main purpose is to enable comprehensive logging of bidirectional network traffic between endpoints. By intercepting and routing all messages through a relay, the system can:
- Log all traffic in both directions
- Timestamp each message
- Record message metadata
- Track connection states
- Monitor protocol compliance
- Generate traffic analytics

## Architecture

### Basic Flow
```
Endpoint X -> [Server Recv Thread] -> Queue A -> [Logger] -> Queue B -> [Client Send Thread] -> Endpoint Y
```

Each thread identifies its relay target using thread labels:
- Server Recv Thread stores Client Send Thread's label
- Client Recv Thread stores Server Send Thread's label
- Actual queue lookup happens at relay time via thread registry

### Message Flow
1. Receiving thread gets data from socket
2. If relay enabled, looks up target queue using stored thread label
3. Pushes message to target queue if found
4. Target send thread processes message normally

### Thread Configuration
```c
CommContext {
    // ... other fields ...
    bool is_relay_enabled;
    const char* foreign_queue_label;  // Label of thread to relay to
}
```

### Queue Resolution
- Threads store only labels, not direct queue pointers
- Queue lookup happens via thread registry at message processing time
- This provides better decoupling between threads
- Allows for dynamic thread creation/destruction

## Logging Components

### Message Logging
- Each message is logged with:
  - Timestamp
  - Direction (X→Y or Y→X)
  - Message size
  - Protocol information
  - Connection identifiers
  - Optional message content

### Traffic Analysis
- Message frequency
- Data volume metrics
- Protocol statistics
- Error rates
- Connection status

### Log Storage
- Per-direction log files
- Rotation policies
- Compression options
- Retention rules

## Configuration

### Logging Settings
```ini
[relay]
mode=transform           # Logging mode
log_content=true        # Whether to log message content
sanitize_sensitive=true # Remove sensitive data before logging
max_log_size=100M      # Log file size before rotation
retention_days=30      # Log retention period

[relay_logging]
log_format=json        # Log format (json, text, binary)
include_metadata=true  # Include connection metadata
compress_logs=true     # Compress rotated logs
```

## Usage Example

```c
// Initialize relay groups with logging
CommsThreadGroup server_group, client_group;
comms_thread_group_init(&server_group, "SERVER_RELAY", &server_sock, &server_flag);
comms_thread_group_init(&client_group, "CLIENT_RELAY", &client_sock, &client_flag);

// Configure logging for both directions
LogConfig_T log_config = {
    .log_content = true,
    .sanitize = true,
    .format = LOG_FORMAT_JSON
};

// Set up relay with logging
CommArgs_T server_args, client_args;
init_relay_with_logging(&server_args, &client_args, &log_config);

// Start relay threads
comms_thread_group_create_threads(&server_group, &server_addr, server_info);
comms_thread_group_create_threads(&client_group, &client_addr, client_info);
```

## Best Practices

1. **Logging Strategy**
   - Define clear logging requirements
   - Set appropriate retention policies
   - Plan storage requirements
   - Consider privacy implications

2. **Performance Considerations**
   - Use appropriate buffer sizes
   - Configure log rotation
   - Monitor disk usage
   - Optimize logging frequency

3. **Security**
   - Sanitize sensitive data
   - Secure log files
   - Control log access
   - Encrypt if needed

4. **Maintenance**
   - Regular log rotation
   - Archive old logs
   - Monitor storage usage
   - Validate log integrity
