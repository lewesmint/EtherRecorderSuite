/**
 * @file logger.c
 * @brief Logging system for the project.
 *
 * Provides functionality for logging messages at various levels,
 * supporting output to a file and standard error.
 */

#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>

#include "platform_time.h"
#include "log_queue.h"
#include "platform_threads.h"
#include "platform_atomic.h"
#include "platform_path.h"
#include "platform_mutex.h"
#include "platform_string.h"
#include "platform_time.h"

#include "thread_registry.h"
#include "utils.h"
#include "app_thread.h"
#include "app_config.h"

#define MAX_LOG_FAILURES 100 // Maximum number of log failures before exiting
#define APP_LOG_FILE_INDEX 0

/* ANSI colour codes (regular CMD on Windows supports limited colours) */
#define ANSI_RESET      "\x1b[0m"
#define ANSI_DEBUG      "\x1b[36m"  // Cyan
#define ANSI_INFO       "\x1b[32m"  // Green
#define ANSI_NOTICE     "\x1b[34m"  // Blue
#define ANSI_WARN       "\x1b[33m"  // Yellow
#define ANSI_ERROR      "\x1b[31m"  // Red
#define ANSI_CRITICAL   "\x1b[35m"  // Magenta
#define ANSI_FATAL      "\x1b[41m"  // Red Background

#define CONFIG_LOG_PATH_KEY "log_file_path"
#define CONFIG_LOG_FILE_KEY "log_file_name"

extern const ThreadConfig ThreadConfigTemplate;

extern bool console_logging_suspended;

// New structure to manage unique file pointers
typedef struct LogFile {
    char file_name[MAX_PATH_LEN];
    FILE *fp;
    bool first_open;
    int ref_count;
} LogFile;

// Table of unique log files
static LogFile log_files[MAX_THREADS];
static int g_log_file_count = 0;

// Modified existing structure to reference LogFile
typedef struct ThreadLogFile {
    char thread_label[MAX_PATH_LEN];
    LogFile *log_file;  // Points to entry in log_files table
    // Removed: FILE *log_fp;
    // Removed: char log_file_name[MAX_PATH_LEN];
    bool first_open;  // Keeping this temporarily until we migrate functionality
} ThreadLogFile;

typedef enum LogTimestampGranularity {
    LOG_TS_NANOSECOND  = 1000000000, // 1/1,000,000,000 (default) 
    LOG_TS_MICROSECOND = 1000000,    // 1/1,000,000
    LOG_TS_MILLISECOND = 1000,       // 1/1,000
    LOG_TS_CENTISECOND = 100,        // 1/100
    LOG_TS_DECISECOND  = 10,         // 1/10
    LOG_TS_SECOND      = 1           // 1/1
} LogTimestampGranularity;

#ifdef _DEBUG
// only relevant in debug builds
bool g_trace_all = false;
#endif

PlatformMutex_T logging_mutex; // Mutex for thread safety
static ThreadLogFile thread_log_files[MAX_THREADS + 1]; // +1 for the main application log file

// Replace Windows-specific timestamp types with platform-agnostic ones
static THREAD_LOCAL int g_timestamp_initialised = 0;

static LogTimestampGranularity g_log_timestamp_granularity = LOG_TS_NANOSECOND;  // Default


#ifdef _DEBUG
static LogLevel g_log_level = LOG_DEBUG;
#else
static LogLevel g_log_level = LOG_INFO;
#endif

static LogOutput g_log_output = LOG_OUTPUT_BOTH; // Log output destination
int g_log_leading_zeros = 12;

//colour is nice 
static bool g_log_use_ansi_colours = false;

// incremented when we learn of a thread that needs logging, gives total number of threads 
// that we need to query for log file names and open file handles
static int g_thread_log_file_count = 0;

// defaults if not read from the config file
static char log_file_path[MAX_PATH_LEN] = "";             // Log file path
static char log_file_name[MAX_PATH_LEN] = "log_file.log"; // Log file name
static off_t g_log_file_size = 10485760;                  // Log file size before rotation

static PlatformThreadHandle log_thread; // Logging thread
static bool logging_thread_started = false; // indicate whether the logger thread has started
static bool g_purge_logs_on_restart = false;
 
void init_logger_mutex(void) {
    /* Initialise the mutex, vital this is down before any logging */
    init_mutex(&logging_mutex);
}
 
 /**
  * @brief Convert a timestamp granularity string to the corresponding enum.
  * @param granularity_str The string representing the timestamp granularity.
  * @param default_granularity The default granularity if the string is invalid.
  * @return The corresponding LogTimestampGranularity value.
  */
 LogTimestampGranularity timestamp_granularity_from_string(const char* granularity_str,
                                                           LogTimestampGranularity default_granularity) {
     if (!granularity_str)
         return default_granularity;
 
     if (strcmp_nocase(granularity_str, "nanosecond") == 0) return LOG_TS_NANOSECOND;
     if (strcmp_nocase(granularity_str, "microsecond") == 0) return LOG_TS_MICROSECOND;
     if (strcmp_nocase(granularity_str, "millisecond") == 0) return LOG_TS_MILLISECOND;
     if (strcmp_nocase(granularity_str, "centisecond") == 0) return LOG_TS_CENTISECOND;
     if (strcmp_nocase(granularity_str, "decisecond") == 0) return LOG_TS_DECISECOND;
     if (strcmp_nocase(granularity_str, "second") == 0) return LOG_TS_SECOND;
 
     return default_granularity;  // Fallback if value is unrecognized
 }
 
 /**
  * @brief Convert a log level string to the corresponding LogLevel enum.
  * @param level_str The string representing the log level.
  * @param default_level The default log level if the string is invalid.
  * @return The corresponding LogLevel value.
  */
 LogLevel log_level_from_string(const char* level_str, LogLevel default_level) {
     if (!level_str) return default_level;
 
     /* Normalize input by making case-insensitive comparisons */
     if (strcmp_nocase(level_str, "trace") == 0) return LOG_TRACE;
     if (strcmp_nocase(level_str, "debug") == 0) return LOG_DEBUG;
     if (strcmp_nocase(level_str, "info") == 0) return LOG_INFO;
     if (strcmp_nocase(level_str, "notice") == 0) return LOG_NOTICE;
 
     /* Allow both "warn" and "warning" */
     if (strcmp_nocase(level_str, "warn") == 0 || strcmp_nocase(level_str, "warning") == 0)
         return LOG_WARN;
 
     /* Allow both "error" and "err" */
     if (strcmp_nocase(level_str, "error") == 0 || strcmp_nocase(level_str, "err") == 0)
         return LOG_ERROR;
 
     if (strcmp_nocase(level_str, "critical") == 0) return LOG_CRITICAL;
 
     /* Allow both "fatal" and "fatal error" */
     if (strcmp_nocase(level_str, "fatal") == 0 || strcmp_nocase(level_str, "fatal error") == 0)
         return LOG_FATAL;
 
     /* Return the default level if the string is invalid */
     return default_level;
 }
 
 
 /**
  * @brief Initialises the high-resolution timer for the current thread.
  */
 // Function to initialise logger's timestamp system
 void init_thread_timestamp_system(void) {
     // Replace Windows-specific code with platform abstraction
     platform_init_timestamp_system();
     g_timestamp_initialised = 1;
 }
 
 
 /**
  * @brief Get the ANSI colour code for a log level.
  */
 static const char* get_log_level_colour(LogLevel level) {
     if (!g_log_use_ansi_colours) return "";  // No colours if disabled
     switch (level) {
     case LOG_DEBUG:    return ANSI_DEBUG;
     case LOG_INFO:     return ANSI_INFO;
     case LOG_NOTICE:   return ANSI_NOTICE;
     case LOG_WARN:     return ANSI_WARN;
     case LOG_ERROR:    return ANSI_ERROR;
     case LOG_CRITICAL: return ANSI_CRITICAL;
     case LOG_FATAL:    return ANSI_FATAL;
     default:           return ""; // No colour
     }
 }
 
 /**
  * @brief Publishes a log entry to the appropriate destination (file or console).
  * @param entry The log entry.
  * @param log_output The file pointer (typically stderr for screen output).
  */
 static void publish_log_entry(const LogEntry_T* entry, FILE* log_output) {
     if (!entry || entry->message[0] == '\0') {
         stream_print(stderr, "Log Error: Attempted to log NULL or blank message\n");
         return;
     }
 
     /* Initialise the timestamp system for the current thread if not already initialised */
     if (!g_timestamp_initialised) {
         // This should never happen, the timing system needs to be initialized
         // before it is first used or it will give a zero or negative offset when
         // g_filetime_reference_ularge is accessed immediately afterwards.
         // So need to call this function as part of a thread's own initialization sequence.
         // If it is not, and initialized here, a slight delay needs to be introduced
         // to avoid the zero/negative offset.
         init_thread_timestamp_system();
         stream_print(stderr, "Log Error: Timestamp system  was not set prior initialised\n");
         sleep_ms(500);
     }
 
     /* Calculate calendar time from high-resolution timestamp */
     time_t rawtime;
     int64_t nanoseconds;
     platform_timestamp_to_calendar_time(&entry->timestamp, &rawtime, &nanoseconds);
 
     /* Format time using platform-independent function */
     struct tm timeinfo;
     platform_localtime(&rawtime, &timeinfo);
 
     /* Calculate fractional part */
     int64_t adjusted_time = nanoseconds / (1000000000 / g_log_timestamp_granularity);
 
     /* Rest of the existing code remains the same */
     int fractional_width = (int)log10((double)g_log_timestamp_granularity);
     int index_width = (g_log_leading_zeros >= 0) ? g_log_leading_zeros : 12;
     const char* log_colour = (log_output == stderr && g_log_use_ansi_colours) ? get_log_level_colour(entry->level) : "";
     const char* reset_colour = (log_output == stderr && g_log_use_ansi_colours) ? "\x1b[0m" : "";
 
     char time_buffer[64];
     strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
 
     /* Print the log entry */
     if (fractional_width > 0) {
         char log_buffer[LOG_MSG_BUFFER_SIZE];
         size_t written = snprintf(log_buffer, sizeof(log_buffer),
             "%0*llu %s.%0*lld %s%s%s: [%s] %s\n",
             index_width, entry->index,
             time_buffer,
             fractional_width, adjusted_time,
             log_colour, log_level_to_string(entry->level), reset_colour,
             entry->thread_label,
             entry->message);
         if (written > 0 && written < sizeof(log_buffer)) {
             stream_print(log_output, "%s", log_buffer);
         }
     }
     else {
         char log_buffer[LOG_MSG_BUFFER_SIZE];
         size_t written = snprintf(log_buffer, sizeof(log_buffer),
             "%0*llu %s %s%s%s: [%s] %s\n",
             index_width, entry->index,
             time_buffer,
             log_colour, log_level_to_string(entry->level), reset_colour,
             entry->thread_label,
             entry->message);
         if (written > 0 && written < sizeof(log_buffer)) {
             stream_print(log_output, "%s", log_buffer);
         }
     }
 
     fflush(log_output);
 }
 
 
 /**
  * @brief Constructs the full log file name.
  * @param full_log_file_name The buffer to store the full log file name.
  * @param size The size of the buffer.
  * @param log_file_path The log file path.
  * @param log_file_name The log file name.
  */
 static void construct_log_file_name(char *full_log_file_name, size_t size, const char *path, const char *name) {
     if (strlen(path) > 0) {
         snprintf(full_log_file_name, size, "%s%c%s", path, PATH_SEPARATOR, name);
     } else {
         full_log_file_name[0] = '\0';
         platform_strcat(full_log_file_name, name, size);
     }
     sanitise_path(full_log_file_name);
 }
 
 
 /**
  * @brief Sets the log file for the current thread.
  * @param filename The log file name to set.
  */
 void set_log_thread_file(const char *label, const char *filename) {
     lock_mutex(&logging_mutex); // Lock the mutex

     if (g_thread_log_file_count >= MAX_THREADS) {
         // Maximum number of threads reached
         unlock_mutex(&logging_mutex); // Unlock the mutex
         return;
     }

     // Initialize thread label
     thread_log_files[g_thread_log_file_count].thread_label[0] = '\0';
     platform_strcat(thread_log_files[g_thread_log_file_count].thread_label, label, 
         sizeof(thread_log_files[g_thread_log_file_count].thread_label));

     // Find or create LogFile entry
     LogFile *log_file = NULL;
     for (int i = 0; i < g_log_file_count; i++) {
         if (strcmp(log_files[i].file_name, filename) == 0) {
             log_file = &log_files[i];
             log_file->ref_count++;
             break;
         }
     }

     if (log_file == NULL && g_log_file_count < MAX_THREADS) {
         log_file = &log_files[g_log_file_count++];
         platform_strcat(log_file->file_name, filename, sizeof(log_file->file_name));
         log_file->fp = NULL;
         log_file->first_open = false;
         log_file->ref_count = 1;
     }

     // Link the ThreadLogFile to the LogFile
     thread_log_files[g_thread_log_file_count].log_file = log_file;
     thread_log_files[g_thread_log_file_count].first_open = false;

     g_thread_log_file_count++;

     unlock_mutex(&logging_mutex); // Unlock the mutex
}

static const char* find_parent_log_file(const char* thread_label) {
    char parent_label[MAX_PATH_LEN];
    char config_key[MAX_PATH_LEN];
    const char* log_file = NULL;
    
    // Make a copy we can modify
    strncpy(parent_label, thread_label, sizeof(parent_label) - 1);
    parent_label[sizeof(parent_label) - 1] = '\0';
    
    // Keep checking parent levels until we find a config or run out of dots
    while (strchr(parent_label, '.') != NULL && !log_file) {
        // Remove the last segment
        char* last_dot = strrchr(parent_label, '.');
        *last_dot = '\0';
        
        // Check if this parent has a log file configured
        snprintf(config_key, sizeof(config_key), "%s." CONFIG_LOG_FILE_KEY, parent_label);
        log_file = get_config_string("logger", config_key, NULL);
    }
    
    return log_file;
}

void set_thread_log_file_from_config(const char* thread_label) {
    char file_config_key[MAX_PATH_LEN];
    const char* config_thread_log_file = NULL;
    const char* config_thread_log_path = get_config_string("logger", CONFIG_LOG_PATH_KEY, NULL);

    /* Read log level from config */
    const char* config_log_level = get_config_string("logger", "log_level", NULL);
    g_log_level = log_level_from_string(config_log_level, g_log_level);

#ifdef _DEBUG
    g_trace_all = get_config_bool("debug", "trace_on", false);
#endif

    // First try the full thread label
    snprintf(file_config_key, sizeof(file_config_key), "%s." CONFIG_LOG_FILE_KEY, thread_label);
    config_thread_log_file = get_config_string("logger", file_config_key, NULL);

    // If not found, try all parent levels
    if (!config_thread_log_file && strchr(thread_label, '.')) {
        config_thread_log_file = find_parent_log_file(thread_label);
    }

    /* Configure log file */
    if (config_thread_log_file) {
        if (config_thread_log_path) {
            char full_log_file_name[MAX_PATH_LEN];
            construct_log_file_name(full_log_file_name, sizeof(full_log_file_name), 
                                  config_thread_log_path, config_thread_log_file);
            set_log_thread_file(thread_label, full_log_file_name);
        }
        else {
            set_log_thread_file(thread_label, config_thread_log_file);
        }
    }
}
 
 
 /**
  * @brief Converts log level to string.
  * @param level The log level.
  * @return The string representation of the log level.
  */
 const char* log_level_to_string(LogLevel level) {
     switch (level) {
         case LOG_DEBUG: return "DEBUG";
         case LOG_INFO: return  "INFO ";
         case LOG_WARN: return  "WARN ";
         case LOG_ERROR: return "ERROR";
         case LOG_FATAL: return "FATAL";
         default: return "UNKNN";
     }
 }
 
 /**
  * @brief Opens the log file and manages the failure count.
  */
 static bool create_log_directory(const char* directory_path, int* creation_failure_count) {
     if (create_directories(directory_path) != 0) {
         if (*creation_failure_count < 5) {
             stream_print(stderr, "Failed to create directory structure for logging: %s\n", directory_path);
             (*creation_failure_count)++;
         }
         return false;
     }
     return true;
 }

 static bool handle_open_failure(const char* filename, int* failure_count) {
     if (*failure_count == 0) {
         stream_print(stderr, "Failed to open log file: %s\n", filename);
     }
     (*failure_count)++;
     // Exit if logging is impossible over 100 iterations
     if (*failure_count >= MAX_LOG_FAILURES) {
         stream_print(stderr, "Unrecoverable failure to open log file: %s\n. Exiting\n", filename);
         exit(EXIT_FAILURE);
     }
     return false;
 }

 static bool open_log_file_if_needed(LogFile *log_file) {
     if (!log_file) {
         return false;
     }

     if (log_file->fp != NULL) {
         return true;  // File already open
     }

     static int log_failure_count = 0;
     static int directory_creation_failure_count = 0;

     // Prepare directory
     char directory_path[MAX_PATH_LEN];
     // Strip the directory path from the full file path
     strip_directory_path(log_file->file_name, directory_path, sizeof(directory_path));
     create_log_directory(directory_path, &directory_creation_failure_count);

     // Open file
     char* mode = g_purge_logs_on_restart ? "w" : "a";
     FILE* fp = NULL;
     PlatformErrorCode err = platform_fopen(&fp, log_file->file_name, mode);
     
     if (err != PLATFORM_ERROR_SUCCESS || fp == NULL) {
         log_file->fp = NULL;
         return handle_open_failure(log_file->file_name, &log_failure_count);
     }

     // Success path
     log_file->fp = fp;
     log_failure_count = 0;

     if (!log_file->first_open) {
         stream_print(stdout, "Successfully opened log file: %s\n", log_file->file_name);
         log_file->first_open = true;
     }

     return true;
 }

static const char* get_rename_error_description(int error_code) {
    switch (error_code) {
        case EACCES: return "Permission denied";
        case ENOENT: return "Source file not found";
        case EEXIST: return "Destination already exists";
        case EINVAL: return "Invalid parameter";
        case ENOSPC: return "No space on device";
        case EROFS:  return "Read-only filesystem";
        case EXDEV:  return "Cross-device link not permitted";
        case EBUSY:  return "File/directory in use";
        case EISDIR: return "Destination is a directory";
        case EMLINK: return "Too many links";
        default:     return "Unknown error";
    }
}

static bool handle_rename_failure(const char* source_file, const char* dest_file, int error_code) {
    const char* error_desc = get_rename_error_description(error_code);
    stream_print(stderr, "Failed to rotate log file from %s to %s: %s (errno: %d)\n", 
                source_file, dest_file, error_desc, error_code);
    return false;
}

static void generate_timestamp_suffix(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, ".%Y%m%d_%H%M%S", t);
}

static void generate_rotated_log_filename(const char* original_filename, char* rotated_log_filename, size_t size) {
    char timestamp[32];
    generate_timestamp_suffix(timestamp, sizeof(timestamp));
    
    // Find the last dot in the original filename
    const char* last_dot = strrchr(original_filename, '.');
    if (last_dot == NULL) {
        // No extension - append timestamp at the end
        snprintf(rotated_log_filename, size, "%s%s", original_filename, timestamp);
    } else {
        // Insert timestamp before the extension
        size_t prefix_len = last_dot - original_filename;
        snprintf(rotated_log_filename, size, "%.*s%s%s", 
                (int)prefix_len, original_filename, 
                timestamp, 
                last_dot);
    }
}

 /**
  * @brief Rotates the log file if it exceeds the configured size.
  */
 static bool rotate_log_file_if_needed(LogFile *log_file) {
     struct stat st;
     if (stat(log_file->file_name, &st) != 0 || st.st_size < g_log_file_size) {
         return true;  // No rotation needed
     }

     // Close current file
     if (log_file->fp != NULL) {
         if (fclose(log_file->fp) != 0) {
             stream_print(stderr, "Failed to close log file: %s (errno: %d)\n", 
                         log_file->file_name, errno);
             log_file->fp = NULL;
             return false;
         }
         log_file->fp = NULL;
     }

     // Generate new filename and rotate
     char rotated_log_filename[MAX_PATH_LEN];
     generate_rotated_log_filename(log_file->file_name, 
                                rotated_log_filename, 
                                sizeof(rotated_log_filename));

     // Rename current file to rotated filename
     if (rename(log_file->file_name, rotated_log_filename) != 0) {
         return handle_rename_failure(log_file->file_name, rotated_log_filename, errno);
     }

     // Open new file
     FILE* fp = NULL;
     PlatformErrorCode err = platform_fopen(&fp, log_file->file_name, "a");
     if (err != PLATFORM_ERROR_SUCCESS || fp == NULL) {
         stream_print(stderr, "Failed to open new log file after rotation: %s\n", 
                     log_file->file_name);
         return false;
     }

     log_file->fp = fp;
     return true;
 }
 
 /**
  * @brief Convert a log destination string to the corresponding LogOutput enum.
  * @param destination_str The string representing the log destination.
  * @param default_output The default log output if the string is invalid.
  * @return The corresponding LogOutput value.
  */
 LogOutput log_output_from_string(const char* destination_str, LogOutput default_output) {
     if (!destination_str) return default_output;
 
     /* Normalize input for case-insensitive comparison */
     if (strcmp_nocase(destination_str, "file") == 0 ||
         strcmp_nocase(destination_str, "log_file") == 0) {
         return LOG_OUTPUT_FILE;
     }
 
     if (strcmp_nocase(destination_str, "console") == 0 ||
         strcmp_nocase(destination_str, "screen") == 0 ||
         strcmp_nocase(destination_str, "terminal") == 0 ||
         strcmp_nocase(destination_str, "stderr") == 0 ||
         strcmp_nocase(destination_str, "stdout") == 0) {
         return LOG_OUTPUT_SCREEN;
     }
 
     if (strcmp_nocase(destination_str, "file and console") == 0 ||
         strcmp_nocase(destination_str, "file_and_console") == 0 ||
         strcmp_nocase(destination_str, "both") == 0 ||
         strcmp_nocase(destination_str, "all") == 0) {
         return LOG_OUTPUT_BOTH;
     }
 
     /* Return the default output if the string is unrecognized */
     return default_output;
 }
 
 
 /**
  * @brief Logs a message immediately to file and console.
  * @param level The log level of the message.
  * @param entry The formatted log message.
  */
void log_immediately(const LogEntry_T* entry) {
    // mutex will have been aquired by the caller
     if (!entry || entry->message[0] == '\0') {
         char error_buffer[LOG_MSG_BUFFER_SIZE];
         size_t written = (size_t)snprintf(error_buffer, sizeof(error_buffer), 
             "Log Error: Attempted to log NULL or blank message\n");
         if (written > 0 && written < sizeof(error_buffer)) {
             stream_print(stderr, "%s", error_buffer);
         }
         return;
     }

     const char* thread_label = entry && entry->thread_label[0] != '\0' ? 
                               entry->thread_label : get_thread_label();
     ThreadLogFile* tlf = &thread_log_files[APP_LOG_FILE_INDEX];

     bool can_log_to_file = true;
     LogOutput current_output = g_log_output;  // Use a temporary variable
    
     /* Check if we have a valid LogFile before attempting file operations */
     if (!tlf->log_file || !tlf->log_file->file_name[0]) {
         can_log_to_file = false;
         current_output = LOG_OUTPUT_SCREEN;  // Temporary fallback to screen logging
     } else {
         /* Rotate & open the main log file if needed */
         if (tlf->log_file->fp) {
             if (!rotate_log_file_if_needed(tlf->log_file)) {
                 can_log_to_file = false;
                 current_output = LOG_OUTPUT_SCREEN;  // Fallback to screen logging
             }
         }
         if (!open_log_file_if_needed(tlf->log_file)) {
             can_log_to_file = false;
             current_output = LOG_OUTPUT_SCREEN;  // Fallback to screen logging
         }

         /* Check if the current thread has a specific log file */
         for (int i = 1; i < g_thread_log_file_count; i++) {
             if (thread_label && strcmp_nocase(thread_log_files[i].thread_label, thread_label) == 0) {
                 if (thread_log_files[i].log_file->fp) {
                     if (!rotate_log_file_if_needed(thread_log_files[i].log_file)) {
                         stream_print(stderr, "File Error: Could not rotate log file for thread %s\n", thread_label);
                     }
                 }
                 if (!open_log_file_if_needed(thread_log_files[i].log_file)) {
                     stream_print(stderr, "File Error: Could not open log file for thread %s\n", thread_label);
                 }
                 tlf = &thread_log_files[i];
                 break;
             }
         }
     }

     /* Log to file if enabled and filename is valid */
     if (can_log_to_file && (current_output == LOG_OUTPUT_FILE || current_output == LOG_OUTPUT_BOTH)) {
         publish_log_entry(entry, tlf->log_file->fp);
     }

     /* Log to screen if enabled */
     if (!console_logging_suspended && (current_output == LOG_OUTPUT_SCREEN || current_output == LOG_OUTPUT_BOTH)) {
         publish_log_entry(entry, stderr);
     }
 }
 
 
 /**
  * @brief Logs a message avoiding the queue
  */
 void log_now(const LogEntry_T *entry) {
     lock_mutex(&logging_mutex);
     log_immediately(entry);
     unlock_mutex(&logging_mutex);
 }
 
 unsigned long long safe_increment_index(void) {
     static PlatformAtomicUInt64 log_index = {0};
     // Use new platform-agnostic atomic fetch-and-add
     return platform_atomic_fetch_add_uint64(&log_index, 1) + 1;
 }
 
 void create_log_entry(LogEntry_T* entry, LogLevel level, const char* message) {
     const char* this_thread_label = get_thread_label();
     const char* name = this_thread_label ? this_thread_label : "UNKNOWN";

     entry->index = safe_increment_index();
     // Use platform-agnostic timestamp function
     platform_get_high_res_timestamp(&entry->timestamp);
     entry->level = level;
     
     // Initialize strings to empty first
     entry->thread_label[0] = '\0';
     entry->message[0] = '\0';
     
     // Copy the thread label and message safely using platform_strcat
     platform_strcat(entry->thread_label, name, sizeof(entry->thread_label));
     platform_strcat(entry->message, message, sizeof(entry->message));
 }
 
 
 void _logger_log(LogLevel level, const char* format, ...) {
     if (level < g_log_level) {
         return;
     }
 
     va_list args;
     va_start(args, format);
 
     // Format the log message
     char log_buffer[LOG_MSG_BUFFER_SIZE];
     vsnprintf(log_buffer, sizeof(log_buffer), format, args);
     va_end(args);
 
     LogEntry_T entry;
     create_log_entry(&entry, level, log_buffer);
 
     if (logging_thread_started) {
         // Push the log message to the queue; if full, log immediately
         if (!log_queue_push(&global_log_queue, &entry)) {
             log_now(&entry);
         }
     } else {
         log_now(&entry);
     }
 }
 
 /**
  * @brief Initialises the logger with the configured log file path, name, and size.
  */
  /**
   * @brief Initialises the logger with the configured log file path, name, and size.
   */
 bool init_logger_from_config(char* logger_init_result) {
     bool success = true;

     lock_mutex(&logging_mutex);

     g_purge_logs_on_restart = get_config_bool("logger", "purge_logs_on_restart", g_purge_logs_on_restart);

     /* Read log destination */
     const char* config_log_destination = get_config_string("logger", "log_destination", NULL);
     g_log_output = log_output_from_string(config_log_destination, LOG_OUTPUT_SCREEN);

     /* Read timestamp granularity */
     const char* config_timestamp_granularity = get_config_string("logger", "timestamp_granularity", NULL);
     g_log_timestamp_granularity = timestamp_granularity_from_string(config_timestamp_granularity, LOG_TS_NANOSECOND);

     /* Read ANSI colour setting */
     g_log_use_ansi_colours = get_config_bool("logger", "ansi_colours", g_log_use_ansi_colours);

     /* make leading zeros on the index for the log message*/
     g_log_leading_zeros = get_config_int("logger", "log_leading_zeros", g_log_leading_zeros);

     /* Read log file size */
     g_log_file_size = get_config_int("logger", "log_file_size", g_log_file_size);

     /* Read log file path and name */
     const char* config_log_file_path = get_config_string("logger", CONFIG_LOG_PATH_KEY, NULL);
     const char* config_log_file_name = get_config_string("logger", CONFIG_LOG_FILE_KEY, NULL);

     /* Use default values if not set */
     if (!config_log_file_path) config_log_file_path = log_file_path;
     if (!config_log_file_name) config_log_file_name = log_file_name;

     /* Check a log filename has been set */
     if (!*config_log_file_name) {
         snprintf(logger_init_result, LOG_MSG_BUFFER_SIZE, 
                 "Logger init failed to obtain a filename for logging from config");
         success = false;
     } else {
         /* Initialize the first LogFile for the main application log */
         LogFile* main_log = &log_files[g_log_file_count++];
         
         /* Construct log file name */
         if (*config_log_file_path) {
             construct_log_file_name(
                 main_log->file_name,
                 sizeof(main_log->file_name),
                 config_log_file_path,
                 config_log_file_name
             );
         } else {
             snprintf(
                 main_log->file_name,
                 sizeof(main_log->file_name),
                 "%s", config_log_file_name
             );
         }

         sanitise_path(main_log->file_name);
         main_log->fp = NULL;
         main_log->first_open = false;

         /* Set up the main ThreadLogFile to point to this LogFile */
         thread_log_files[APP_LOG_FILE_INDEX].log_file = main_log;
         thread_log_files[APP_LOG_FILE_INDEX].thread_label[0] = '\0';  // Main log has no specific thread
     }

     /* Initialize log queue */
     log_queue_init(&global_log_queue);

     /* Start logging thread regardless of success */
     logging_thread_started = true;

     /* Log initialization message */
     if (success) {
         snprintf(logger_init_result, LOG_MSG_BUFFER_SIZE, "Logger initialised. App logging to %s",
             thread_log_files[APP_LOG_FILE_INDEX].log_file->file_name);
     }

     g_thread_log_file_count++;

     unlock_mutex(&logging_mutex);

     return success;
 }
 
 
 /**
  * @brief Sets the log level.
  * @param level The log level to set.
  */
 void logger_set_level(LogLevel level) {
     g_log_level = level;
 }
 
 /**
  * @brief Sets the log output destination.
  * @param output The log output destination to set.
  */
 void logger_set_output(LogOutput output) {
     g_log_output = output;
 }
 
 /**
  * @brief Closes the logger and releases resources.
  */
 void logger_close(void) {
     lock_mutex(&logging_mutex);
     // Close all unique log files
     for (int i = 0; i < g_log_file_count; i++) {
         if (log_files[i].fp) {
             fclose(log_files[i].fp);
             log_files[i].fp = NULL;
         }
     }
     g_log_file_count = 0;
     g_thread_log_file_count = 0;  // Reset thread counter for completeness

     unlock_mutex(&logging_mutex);

     if (logging_thread_started) {
         // Wait for logging thread to finish (it won't, so this is just for completeness)
         platform_thread_join(log_thread, NULL);
     }
 }
 
 LogLevel logger_get_level(void) {
     return g_log_level;
 }
 
 const char* get_level_name(LogLevel level) {
     return log_level_to_string(level);
 }

static void* logger_thread_function(void* arg) {
    // printf("Logger thread started\n");
    (void)arg;
    logger_log(LOG_INFO, "Logger thread started");

    // No more condition/flag needed - thread registry state is enough
    LogEntry_T entry;
 
    while (!shutdown_signalled()) {
        while (log_queue_pop(&global_log_queue, &entry)) {
            if (*entry.thread_label == '\0')
               printf("Logger thread processing log from: NULL\n");
            log_now(&entry);
        }
        // sleep_ms(1);
    }

    PlatformWaitResult wait_result = thread_registry_wait_others();
    if (wait_result != PLATFORM_WAIT_SUCCESS) {
        logger_log(LOG_WARN, "Logger thread failed to wait for other threads: %d", wait_result);
    }
    
    logger_log(LOG_INFO, "Logger thread shutting down.");
    stream_print(stdout, "Logger thread bye bye.\n");
    return (void*)THREAD_SUCCESS;
}

static ThreadConfig logger_thread;

ThreadConfig* get_logger_thread(void) {
    static bool initialized = false;
    if (!initialized) {
        logger_thread = ThreadConfigTemplate;
        logger_thread.label = "LOGGER";
        logger_thread.func = logger_thread_function;
        logger_thread.suppressed = false;  // Logger cannot be suppressed
        initialized = true;
    }
    return &logger_thread;
}
