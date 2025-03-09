# Claude Ether API Documentation

## Core APIs

### Client API
```c
// Initialize a client connection
ClientHandle_T client_init(const char* host, uint16_t port);

// Start client communication threads
int client_start(ClientHandle_T handle);

// Send data to server
int client_send(ClientHandle_T handle, const void* data, size_t length);

// Register receive callback
int client_set_receive_callback(ClientHandle_T handle, ReceiveCallback_T callback);

// Cleanup client resources
void client_cleanup(ClientHandle_T handle);
```

### Server API
```c
// Initialize server on specified port
ServerHandle_T server_init(uint16_t port);

// Start accepting connections
int server_start(ServerHandle_T handle);

// Broadcast to all connected clients
int server_broadcast(ServerHandle_T handle, const void* data, size_t length);

// Register new connection callback
int server_set_connection_callback(ServerHandle_T handle, ConnectionCallback_T callback);

// Cleanup server resources
void server_cleanup(ServerHandle_T handle);
```

### Thread Management API
```c
// Create a new thread group
ThreadGroupHandle_T thread_group_create(const char* name);

// Add thread to group
int thread_group_add(ThreadGroupHandle_T group, AppThread_T* thread);

// Signal group shutdown
void thread_group_shutdown(ThreadGroupHandle_T group);
```

## Error Handling
All functions return error codes defined in `platform_error.h`. Zero indicates success, negative values indicate errors.

## Thread Safety
- All API functions are thread-safe unless explicitly noted
- Callbacks are invoked from worker threads
- Resource cleanup functions must not be called from callbacks

## Usage Examples

### Basic Client
```c
ClientHandle_T client = client_init("localhost", 8080);
client_set_receive_callback(client, on_receive);
client_start(client);
// ... use client ...
client_cleanup(client);
```

### Basic Server
```c
ServerHandle_T server = server_init(8080);
server_set_connection_callback(server, on_client_connect);
server_start(server);
// ... run server ...
server_cleanup(server);
```