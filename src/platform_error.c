/**
 * @file platform_error.c
 * @brief Implementation of platform-agnostic error handling utilities
 */

 #include "platform_error.h"
 #include <stdio.h>
 #include <string.h>
 
 #ifdef _WIN32
 #include <windows.h>
 #include <winsock2.h>
 #else
 #include <errno.h>
 #include <string.h>
 #endif
 
 /**
  * @brief Utility function to sanitize error messages
  * 
  * Removes trailing whitespace, newlines, carriage returns, and periods.
  * 
  * @param message The message to sanitize
  * @return char* Pointer to the sanitized message
  */
 static char* sanitize_error_message(char* message) {
     if (!message) {
         return message;
     }
     
     size_t len = strlen(message);
     
     // Trim trailing spaces, newlines, carriage returns, and periods
     while (len > 0 && (message[len-1] == ' ' || 
                        message[len-1] == '\n' || 
                        message[len-1] == '\r' || 
                        message[len-1] == '.')) {
         message[--len] = '\0';
     }
     
     return message;
 }
 
 /**
  * @brief Maps common Windows system error codes to platform-agnostic error codes
  * 
  * @param error_code The Windows system error code
  * @param domain The error domain for the error
  * @return PlatformErrorCode The platform-agnostic error code
  */
 #ifdef _WIN32
 static PlatformErrorCode map_windows_error_code(DWORD error_code, PlatformErrorDomain domain) {
     switch (domain) {
         case PLATFORM_ERROR_DOMAIN_SOCKET:
             switch (error_code) {
                 case WSAEACCES:               return PLATFORM_ERROR_PERMISSION_DENIED;
                 case WSAEADDRINUSE:           return PLATFORM_ERROR_SOCKET_BIND;
                 case WSAEADDRNOTAVAIL:        return PLATFORM_ERROR_SOCKET_BIND;
                 case WSAEAFNOSUPPORT:         return PLATFORM_ERROR_NOT_SUPPORTED;
                 case WSAEALREADY:             return PLATFORM_ERROR_SOCKET_CONNECT;
                 case WSAECONNABORTED:         return PLATFORM_ERROR_SOCKET_CLOSED;
                 case WSAECONNREFUSED:         return PLATFORM_ERROR_SOCKET_CONNECT;
                 case WSAECONNRESET:           return PLATFORM_ERROR_SOCKET_CLOSED;
                 case WSAEFAULT:               return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case WSAEHOSTUNREACH:         return PLATFORM_ERROR_HOST_NOT_FOUND;
                 case WSAEINPROGRESS:          return PLATFORM_ERROR_SOCKET_CONNECT;
                 case WSAEINTR:                return PLATFORM_ERROR_UNKNOWN;
                 case WSAEINVAL:               return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case WSAEISCONN:              return PLATFORM_ERROR_SOCKET_CONNECT;
                 case WSAEMFILE:               return PLATFORM_ERROR_SOCKET_CREATE;
                 case WSAENETDOWN:             return PLATFORM_ERROR_SOCKET_CONNECT;
                 case WSAENETRESET:            return PLATFORM_ERROR_SOCKET_CLOSED;
                 case WSAENETUNREACH:          return PLATFORM_ERROR_HOST_NOT_FOUND;
                 case WSAENOBUFS:              return PLATFORM_ERROR_MEMORY_ALLOC;
                 case WSAENOPROTOOPT:          return PLATFORM_ERROR_NOT_SUPPORTED;
                 case WSAENOTCONN:             return PLATFORM_ERROR_SOCKET_CLOSED;
                 case WSAENOTSOCK:             return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case WSAEOPNOTSUPP:           return PLATFORM_ERROR_NOT_SUPPORTED;
                 case WSAEPROTONOSUPPORT:      return PLATFORM_ERROR_NOT_SUPPORTED;
                 case WSAEPROTOTYPE:           return PLATFORM_ERROR_NOT_SUPPORTED;
                 case WSAETIMEDOUT:            return PLATFORM_ERROR_TIMEOUT;
                 case WSAEWOULDBLOCK:          return PLATFORM_ERROR_TIMEOUT;
                 case WSAHOST_NOT_FOUND:       return PLATFORM_ERROR_HOST_NOT_FOUND;
                 case WSANOTINITIALISED:       return PLATFORM_ERROR_NOT_IMPLEMENTED;
                 case WSANO_DATA:              return PLATFORM_ERROR_HOST_NOT_FOUND;
                 case WSANO_RECOVERY:          return PLATFORM_ERROR_UNKNOWN;
                 case WSASYSNOTREADY:          return PLATFORM_ERROR_NOT_IMPLEMENTED;
                 case WSATRY_AGAIN:            return PLATFORM_ERROR_HOST_NOT_FOUND;
                 case WSAVERNOTSUPPORTED:      return PLATFORM_ERROR_NOT_SUPPORTED;
                 default:                      return PLATFORM_ERROR_UNKNOWN;
             }
             
         case PLATFORM_ERROR_DOMAIN_THREAD:
             switch (error_code) {
                 case ERROR_INVALID_HANDLE:    return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case ERROR_INVALID_PARAMETER: return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case ERROR_ACCESS_DENIED:     return PLATFORM_ERROR_PERMISSION_DENIED;
                 case ERROR_NOT_ENOUGH_MEMORY: return PLATFORM_ERROR_MEMORY_ALLOC;
                 case ERROR_OUTOFMEMORY:       return PLATFORM_ERROR_MEMORY_ALLOC;
                 case ERROR_TIMEOUT:           return PLATFORM_ERROR_TIMEOUT;
                 default:                      return PLATFORM_ERROR_UNKNOWN;
             }
             
         case PLATFORM_ERROR_DOMAIN_FILE:
             switch (error_code) {
                 case ERROR_FILE_NOT_FOUND:    return PLATFORM_ERROR_FILE_NOT_FOUND;
                 case ERROR_PATH_NOT_FOUND:    return PLATFORM_ERROR_DIRECTORY_NOT_FOUND;
                 case ERROR_ACCESS_DENIED:     return PLATFORM_ERROR_PERMISSION_DENIED;
                 case ERROR_INVALID_HANDLE:    return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case ERROR_SHARING_VIOLATION: return PLATFORM_ERROR_FILE_ACCESS;
                 case ERROR_LOCK_VIOLATION:    return PLATFORM_ERROR_FILE_ACCESS;
                 case ERROR_HANDLE_DISK_FULL:  return PLATFORM_ERROR_FILE_ACCESS;
                 case ERROR_DISK_FULL:         return PLATFORM_ERROR_FILE_ACCESS;
                 case ERROR_WRITE_PROTECT:     return PLATFORM_ERROR_PERMISSION_DENIED;
                 default:                      return PLATFORM_ERROR_UNKNOWN;
             }
             
         case PLATFORM_ERROR_DOMAIN_MEMORY:
             switch (error_code) {
                 case ERROR_NOT_ENOUGH_MEMORY: return PLATFORM_ERROR_MEMORY_ALLOC;
                 case ERROR_OUTOFMEMORY:       return PLATFORM_ERROR_MEMORY_ALLOC;
                 case ERROR_INVALID_ADDRESS:   return PLATFORM_ERROR_MEMORY_ACCESS;
                 case ERROR_INVALID_PARAMETER: return PLATFORM_ERROR_INVALID_ARGUMENT;
                 default:                      return PLATFORM_ERROR_UNKNOWN;
             }
             
         case PLATFORM_ERROR_DOMAIN_GENERAL:
         default:
             switch (error_code) {
                 case ERROR_SUCCESS:              return PLATFORM_ERROR_SUCCESS;
                 case ERROR_INVALID_PARAMETER:    return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case ERROR_NOT_SUPPORTED:        return PLATFORM_ERROR_NOT_SUPPORTED;
                 case ERROR_CALL_NOT_IMPLEMENTED: return PLATFORM_ERROR_NOT_IMPLEMENTED;
                 case ERROR_ACCESS_DENIED:        return PLATFORM_ERROR_PERMISSION_DENIED;
                 case ERROR_TIMEOUT:              return PLATFORM_ERROR_TIMEOUT;
                 default:                         return PLATFORM_ERROR_UNKNOWN;
             }
     }
 }
 #else
 /**
  * @brief Maps common POSIX error codes to platform-agnostic error codes
  * 
  * @param error_code The POSIX error code (errno)
  * @param domain The error domain for the error
  * @return PlatformErrorCode The platform-agnostic error code
  */
 static PlatformErrorCode map_posix_error_code(int error_code, PlatformErrorDomain domain) {
     switch (domain) {
         case PLATFORM_ERROR_DOMAIN_SOCKET:
             switch (error_code) {
                 case 0:                 return PLATFORM_ERROR_SUCCESS;
                 case EACCES:            return PLATFORM_ERROR_PERMISSION_DENIED;
                 case EADDRINUSE:        return PLATFORM_ERROR_SOCKET_BIND;
                 case EADDRNOTAVAIL:     return PLATFORM_ERROR_SOCKET_BIND;
                 case EAFNOSUPPORT:      return PLATFORM_ERROR_NOT_SUPPORTED;
                 case EALREADY:          return PLATFORM_ERROR_SOCKET_CONNECT;
                 case EBADF:             return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case ECONNABORTED:      return PLATFORM_ERROR_SOCKET_CLOSED;
                 case ECONNREFUSED:      return PLATFORM_ERROR_SOCKET_CONNECT;
                 case ECONNRESET:        return PLATFORM_ERROR_SOCKET_CLOSED;
                 case EDESTADDRREQ:      return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case EFAULT:            return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case EHOSTUNREACH:      return PLATFORM_ERROR_HOST_NOT_FOUND;
                 case EINPROGRESS:       return PLATFORM_ERROR_SOCKET_CONNECT;
                 case EINTR:             return PLATFORM_ERROR_UNKNOWN;
                 case EINVAL:            return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case EISCONN:           return PLATFORM_ERROR_SOCKET_CONNECT;
                 case EMFILE:            return PLATFORM_ERROR_SOCKET_CREATE;
                 case ENETDOWN:          return PLATFORM_ERROR_SOCKET_CONNECT;
                 case ENETRESET:         return PLATFORM_ERROR_SOCKET_CLOSED;
                 case ENETUNREACH:       return PLATFORM_ERROR_HOST_NOT_FOUND;
                 case ENOBUFS:           return PLATFORM_ERROR_MEMORY_ALLOC;
                 case ENOPROTOOPT:       return PLATFORM_ERROR_NOT_SUPPORTED;
                 case ENOTCONN:          return PLATFORM_ERROR_SOCKET_CLOSED;
                 case ENOTSOCK:          return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case EOPNOTSUPP:        return PLATFORM_ERROR_NOT_SUPPORTED;
                 case EPROTONOSUPPORT:   return PLATFORM_ERROR_NOT_SUPPORTED;
                 case EPROTOTYPE:        return PLATFORM_ERROR_NOT_SUPPORTED;
                 case ETIMEDOUT:         return PLATFORM_ERROR_TIMEOUT;
                 case EWOULDBLOCK:       return PLATFORM_ERROR_TIMEOUT;
                 default:                return PLATFORM_ERROR_UNKNOWN;
             }
             
         case PLATFORM_ERROR_DOMAIN_THREAD:
             switch (error_code) {
                 case 0:                 return PLATFORM_ERROR_SUCCESS;
                 case EAGAIN:            return PLATFORM_ERROR_THREAD_CREATE;
                 case EINVAL:            return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case EPERM:             return PLATFORM_ERROR_PERMISSION_DENIED;
                 case ESRCH:             return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case EDEADLK:           return PLATFORM_ERROR_MUTEX_LOCK;
                 case ENOMEM:            return PLATFORM_ERROR_MEMORY_ALLOC;
                 case EBUSY:             return PLATFORM_ERROR_MUTEX_LOCK;
                 case ETIMEDOUT:         return PLATFORM_ERROR_TIMEOUT;
                 default:                return PLATFORM_ERROR_UNKNOWN;
             }
             
         case PLATFORM_ERROR_DOMAIN_FILE:
             switch (error_code) {
                 case 0:                 return PLATFORM_ERROR_SUCCESS;
                 case EACCES:            return PLATFORM_ERROR_PERMISSION_DENIED;
                 case EEXIST:            return PLATFORM_ERROR_FILE_EXISTS;
                 case EFAULT:            return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case EINVAL:            return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case EISDIR:            return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case ELOOP:             return PLATFORM_ERROR_FILE_ACCESS;
                 case EMFILE:            return PLATFORM_ERROR_FILE_ACCESS;
                 case ENAMETOOLONG:      return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case ENOENT:            return PLATFORM_ERROR_FILE_NOT_FOUND;
                 case ENOMEM:            return PLATFORM_ERROR_MEMORY_ALLOC;
                 case ENOSPC:            return PLATFORM_ERROR_FILE_ACCESS;
                 case ENOTDIR:           return PLATFORM_ERROR_DIRECTORY_NOT_FOUND;
                 case EPERM:             return PLATFORM_ERROR_PERMISSION_DENIED;
                 case EROFS:             return PLATFORM_ERROR_PERMISSION_DENIED;
                 default:                return PLATFORM_ERROR_UNKNOWN;
             }
             
         case PLATFORM_ERROR_DOMAIN_MEMORY:
             switch (error_code) {
                 case 0:                 return PLATFORM_ERROR_SUCCESS;
                 case ENOMEM:            return PLATFORM_ERROR_MEMORY_ALLOC;
                 case EFAULT:            return PLATFORM_ERROR_MEMORY_ACCESS;
                 case EINVAL:            return PLATFORM_ERROR_INVALID_ARGUMENT;
                 default:                return PLATFORM_ERROR_UNKNOWN;
             }
             
         case PLATFORM_ERROR_DOMAIN_GENERAL:
         default:
             switch (error_code) {
                 case 0:                 return PLATFORM_ERROR_SUCCESS;
                 case EINVAL:            return PLATFORM_ERROR_INVALID_ARGUMENT;
                 case ENOTSUP:           return PLATFORM_ERROR_NOT_SUPPORTED;
                 case ENOSYS:            return PLATFORM_ERROR_NOT_IMPLEMENTED;
                 case EPERM:             return PLATFORM_ERROR_PERMISSION_DENIED;
                 case EACCES:            return PLATFORM_ERROR_PERMISSION_DENIED;
                 case ETIMEDOUT:         return PLATFORM_ERROR_TIMEOUT;
                 default:                return PLATFORM_ERROR_UNKNOWN;
             }
     }
 }
 #endif
 
 PlatformErrorCode platform_get_error_code(PlatformErrorDomain domain) {
 #ifdef _WIN32
     DWORD error_code = GetLastError();
     return map_windows_error_code(error_code, domain);
 #else
     int error_code = errno;
     return map_posix_error_code(error_code, domain);
 #endif
 }
 
 PlatformErrorCode platform_get_socket_error_code(void) {
 #ifdef _WIN32
     DWORD error_code = WSAGetLastError();
     return map_windows_error_code(error_code, PLATFORM_ERROR_DOMAIN_SOCKET);
 #else
     int error_code = errno;
     return map_posix_error_code(error_code, PLATFORM_ERROR_DOMAIN_SOCKET);
 #endif
 }
 
 char* platform_get_error_message_from_code(PlatformErrorCode error_code, char* buffer, size_t buffer_size) {
     if (!buffer || buffer_size == 0) {
         return NULL;
     }
     
     // Provide consistent, human-readable messages for each error code
     switch (error_code) {
         case PLATFORM_ERROR_SUCCESS:
             snprintf(buffer, buffer_size, "Success");
             break;
         case PLATFORM_ERROR_UNKNOWN:
             snprintf(buffer, buffer_size, "Unknown error");
             break;
         case PLATFORM_ERROR_INVALID_ARGUMENT:
             snprintf(buffer, buffer_size, "Invalid argument");
             break;
         case PLATFORM_ERROR_NOT_IMPLEMENTED:
             snprintf(buffer, buffer_size, "Not implemented");
             break;
         case PLATFORM_ERROR_NOT_SUPPORTED:
             snprintf(buffer, buffer_size, "Not supported");
             break;
         case PLATFORM_ERROR_PERMISSION_DENIED:
             snprintf(buffer, buffer_size, "Permission denied");
             break;
         case PLATFORM_ERROR_TIMEOUT:
             snprintf(buffer, buffer_size, "Operation timed out");
             break;
         case PLATFORM_ERROR_SOCKET_CREATE:
             snprintf(buffer, buffer_size, "Failed to create socket");
             break;
         case PLATFORM_ERROR_SOCKET_BIND:
             snprintf(buffer, buffer_size, "Failed to bind socket");
             break;
         case PLATFORM_ERROR_SOCKET_CONNECT:
             snprintf(buffer, buffer_size, "Failed to connect socket");
             break;
         case PLATFORM_ERROR_SOCKET_LISTEN:
             snprintf(buffer, buffer_size, "Failed to listen on socket");
             break;
         case PLATFORM_ERROR_SOCKET_ACCEPT:
             snprintf(buffer, buffer_size, "Failed to accept connection");
             break;
         case PLATFORM_ERROR_SOCKET_SEND:
             snprintf(buffer, buffer_size, "Failed to send data");
             break;
         case PLATFORM_ERROR_SOCKET_RECEIVE:
             snprintf(buffer, buffer_size, "Failed to receive data");
             break;
         case PLATFORM_ERROR_SOCKET_CLOSED:
             snprintf(buffer, buffer_size, "Socket is closed");
             break;
         case PLATFORM_ERROR_HOST_NOT_FOUND:
             snprintf(buffer, buffer_size, "Host not found");
             break;
         case PLATFORM_ERROR_THREAD_CREATE:
             snprintf(buffer, buffer_size, "Failed to create thread");
             break;
         case PLATFORM_ERROR_THREAD_JOIN:
             snprintf(buffer, buffer_size, "Failed to join thread");
             break;
         case PLATFORM_ERROR_THREAD_DETACH:
             snprintf(buffer, buffer_size, "Failed to detach thread");
             break;
         case PLATFORM_ERROR_MUTEX_INIT:
             snprintf(buffer, buffer_size, "Failed to initialize mutex");
             break;
         case PLATFORM_ERROR_MUTEX_LOCK:
             snprintf(buffer, buffer_size, "Failed to lock mutex");
             break;
         case PLATFORM_ERROR_MUTEX_UNLOCK:
             snprintf(buffer, buffer_size, "Failed to unlock mutex");
             break;
         case PLATFORM_ERROR_CONDITION_INIT:
             snprintf(buffer, buffer_size, "Failed to initialize condition variable");
             break;
         case PLATFORM_ERROR_CONDITION_WAIT:
             snprintf(buffer, buffer_size, "Failed to wait on condition variable");
             break;
         case PLATFORM_ERROR_CONDITION_SIGNAL:
             snprintf(buffer, buffer_size, "Failed to signal condition variable");
             break;
         case PLATFORM_ERROR_FILE_NOT_FOUND:
             snprintf(buffer, buffer_size, "File not found");
             break;
         case PLATFORM_ERROR_FILE_EXISTS:
             snprintf(buffer, buffer_size, "File already exists");
             break;
         case PLATFORM_ERROR_FILE_ACCESS:
             snprintf(buffer, buffer_size, "File access error");
             break;
         case PLATFORM_ERROR_DIRECTORY_NOT_FOUND:
             snprintf(buffer, buffer_size, "Directory not found");
             break;
         case PLATFORM_ERROR_DIRECTORY_EXISTS:
             snprintf(buffer, buffer_size, "Directory already exists");
             break;
         case PLATFORM_ERROR_DIRECTORY_ACCESS:
             snprintf(buffer, buffer_size, "Directory access error");
             break;
         case PLATFORM_ERROR_MEMORY_ALLOC:
             snprintf(buffer, buffer_size, "Memory allocation error");
             break;
         case PLATFORM_ERROR_MEMORY_FREE:
             snprintf(buffer, buffer_size, "Memory free error");
             break;
         case PLATFORM_ERROR_MEMORY_ACCESS:
             snprintf(buffer, buffer_size, "Memory access error");
             break;
         default:
             snprintf(buffer, buffer_size, "Error code %d", error_code);
             break;
     }
     
     return buffer;
 }
 
 char* platform_get_error_message(PlatformErrorDomain domain, char* buffer, size_t buffer_size) {
     if (!buffer || buffer_size == 0) {
         return NULL;
     }
     
     // Get the platform-agnostic error code first
     PlatformErrorCode error_code = platform_get_error_code(domain);
     
     // Get the system-specific error message
 #ifdef _WIN32
     DWORD win_error_code = GetLastError();
     LPSTR sys_msg_buf = NULL;
     DWORD result = FormatMessageA(
         FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
         NULL,
         win_error_code,
         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
         (LPSTR)&sys_msg_buf,
         0,
         NULL);
     
     if (result == 0 || sys_msg_buf == NULL) {
         // FormatMessage failed, use our platform-agnostic message
         platform_get_error_message_from_code(error_code, buffer, buffer_size);
     } else {
         // Format the message with the error code and system message
         char temp_buffer[256];
         platform_get_error_message_from_code(error_code, temp_buffer, sizeof(temp_buffer));
         snprintf(buffer, buffer_size, "%s (system error %lu: %s)", 
             temp_buffer,
             win_error_code,
             sanitize_error_message(sys_msg_buf));
         LocalFree(sys_msg_buf);
     }
 #else
     int posix_error_code = errno;
     
     // Use the thread-safe version of strerror
     #if defined(_GNU_SOURCE)
         // GNU version returns a char* and ignores the buffer
         char sys_msg_buf[256];
         char* sys_msg = strerror_r(posix_error_code, sys_msg_buf, sizeof(sys_msg_buf));
         
         // Format the message with the error code and system message
         char temp_buffer[256];
         platform_get_error_message_from_code(error_code, temp_buffer, sizeof(temp_buffer));
         snprintf(buffer, buffer_size, "%s (system error %d: %s)", 
             temp_buffer,
             posix_error_code,
             sanitize_error_message(sys_msg));
     #else
         // POSIX version returns an int and uses the buffer
         char sys_msg_buf[256];
         if (strerror_r(posix_error_code, sys_msg_buf, sizeof(sys_msg_buf)) != 0) {
             // strerror_r failed, use our platform-agnostic message only
             platform_get_error_message_from_code(error_code, buffer, buffer_size);
         } else {
             // Format the message with the error code and system message
             char temp_buffer[256];
             platform_get_error_message_from_code(error_code, temp_buffer, sizeof(temp_buffer));
             snprintf(buffer, buffer_size, "%s (system error %d: %s)", 
                 temp_buffer,
                 posix_error_code,
                 sanitize_error_message(sys_msg_buf));
         }
     #endif
 #endif
     
     return buffer;
 }
 
 char* platform_get_socket_error_message(char* buffer, size_t buffer_size) {
     if (!buffer || buffer_size == 0) {
         return NULL;
     }
     
     // Get the platform-agnostic error code first
     PlatformErrorCode error_code = platform_get_socket_error_code();
     
     // Get the system-specific error message
 #ifdef _WIN32
     DWORD win_error_code = WSAGetLastError();
     LPSTR sys_msg_buf = NULL;
     DWORD result = FormatMessageA(
         FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
         NULL,
         win_error_code,
         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
         (LPSTR)&sys_msg_buf,
         0,
         NULL);
     
     if (result == 0 || sys_msg_buf == NULL) {
         // FormatMessage failed, use our platform-agnostic message
         platform_get_error_message_from_code(error_code, buffer, buffer_size);
     } else {
         // Format the message with the error code and system message
         char temp_buffer[256];
         platform_get_error_message_from_code(error_code, temp_buffer, sizeof(temp_buffer));
         snprintf(buffer, buffer_size, "%s (socket error %lu: %s)", 
             temp_buffer,
             win_error_code,
             sanitize_error_message(sys_msg_buf));
         LocalFree(sys_msg_buf);
     }
 #else
     int posix_error_code = errno;
     
     // Use the thread-safe version of strerror
     #if defined(_GNU_SOURCE)
         // GNU version returns a char* and ignores the buffer
         char sys_msg_buf[256];
         char* sys_msg = strerror_r(posix_error_code, sys_msg_buf, sizeof(sys_msg_buf));
         
         // Format the message with the error code and system message
         char temp_buffer[256];
         platform_get_error_message_from_code(error_code, temp_buffer, sizeof(temp_buffer));
         snprintf(buffer, buffer_size, "%s (socket error %d: %s)", 
             temp_buffer,
             posix_error_code,
             sanitize_error_message(sys_msg));
     #else
         // POSIX version returns an int and uses the buffer
         char sys_msg_buf[256];
         if (strerror_r(posix_error_code, sys_msg_buf, sizeof(sys_msg_buf)) != 0) {
             // strerror_r failed, use our platform-agnostic message only
             platform_get_error_message_from_code(error_code, buffer, buffer_size);
         } else {
             // Format the message with the error code and system message
             char temp_buffer[256];
             platform_get_error_message_from_code(error_code, temp_buffer, sizeof(temp_buffer));
             snprintf(buffer, buffer_size, "%s (socket error %d: %s)", 
                 temp_buffer,
                 posix_error_code,
                 sanitize_error_message(sys_msg_buf));
         }
     #endif
 #endif
     
     return buffer;
 }
