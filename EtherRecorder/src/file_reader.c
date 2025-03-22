#include "file_reader.h"
#include "platform_threads.h"
#include "platform_string.h"
#include "platform_error.h"
#include "platform_file.h"
#include "message_types.h"
#include "logger.h"
#include "app_config.h"
#include "thread_registry.h"
#include "shutdown_handler.h"
#include "thread_status_errors.h"
#include "utils.h"


typedef struct PlatformFile* FileHandle;

static PlatformFileHandle open_file(const char* filepath, PlatformErrorCode* error_code) {
    return platform_file_open(
        filepath,
        PLATFORM_FILE_READ,      // We only need read access
        PLATFORM_FILE_SHARE_READ,// Allow other processes to read
        error_code
    );
}

static bool read_file_chunk(FileHandle handle, void* buffer, size_t size,
    size_t* bytes_read, const char** error_msg) {
    FILE* file = (FILE*)handle;
    *bytes_read = fread(buffer, 1, size, file);
    if (ferror(file)) {
        *error_msg = "Error reading file";
        return false;
    }
    return true;
}

static void close_file(FileHandle handle) {
    if (handle) {
        fclose((FILE*)handle);
    }
}

static bool get_file_size(FileHandle handle, size_t* size) {
    FILE* file = (FILE*)handle;
    if (fseek(file, 0, SEEK_END) != 0) {
        return false;
    }
    *size = ftell(file);
    if (fseek(file, 0, SEEK_SET) != 0) {
        return false;
    }
    return true;
}


// Default file reader configuration
static const FileReaderConfig DefaultFileReaderConfig = {
    .read_mode = FILE_READ_ONCE,
    .chunk_size = 0,                  // Use MAX_MESSAGE_SIZE
    .chunk_delay_ms = 0,              // No delay
    .reload_delay_ms = 1000,          // 1 second between reloads
    .queue_timeout_ms = DEFAULT_THREAD_WAIT_TIMEOUT_MS,
    .max_queue_size = 0,              // Unlimited
    .block_when_full = true,
    .log_progress = true,
    .progress_interval_ms = 1000      // Log progress every second
};

void* file_reader_thread_function(void*);

ThreadConfig* get_file_reader_thread(const char* filepath, const char* target_thread_label) {
    static ThreadConfig file_reader;
    static FileReaderConfig reader_config;
    static bool initialized = false;

    if (!initialized) {
        reader_config = DefaultFileReaderConfig;  // Initialize here instead
        reader_config.filepath = filepath;
        reader_config.foreign_thread_label = target_thread_label;
        reader_config.chunk_delay_ms = get_config_int("server", "file_chunk_delay_ms", 0);

        file_reader = create_thread_config(
            "FILE_READER",
            file_reader_thread_function,
            &reader_config);
        initialized = true;
    }

    return &file_reader;
}

FileReadMode string_to_read_mode(const char* file_read_mode) {
    for (int i = 0; i < sizeof(FILE_READ_MODE_STRINGS) / sizeof(FILE_READ_MODE_STRINGS[0]); i++) {
        if (strcmp_nocase(file_read_mode, FILE_READ_MODE_STRINGS[i]) == 0) {
            return (FileReadMode)i;
        }
    }
    return FILE_READ_ONCE;  // Default return value if no match found
}

char* read_mode_to_string(FileReadMode mode) {
    return (char*)FILE_READ_MODE_STRINGS[mode];
}

static const char* get_config_read_mode(const FileReaderConfig* config) {
    return get_config_string("file_reader", "read_mode", read_mode_to_string(config->read_mode));
}

static PlatformErrorCode thread_reader_init_config(FileReaderConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    const char* mode_str = get_config_read_mode(config);
    config->read_mode = string_to_read_mode(mode_str);

    config->chunk_size = get_config_int("file_reader", "chunk_size", config->chunk_size);
    config->chunk_delay_ms = get_config_int("file_reader", "chunk_delay_ms", config->chunk_delay_ms);
    config->reload_delay_ms = get_config_int("file_reader", "reload_delay_ms", config->reload_delay_ms);
    config->queue_timeout_ms = get_config_int("file_reader", "queue_timeout_ms", config->queue_timeout_ms);
    config->max_queue_size = get_config_int("file_reader", "max_queue_size", config->max_queue_size);
    config->block_when_full = get_config_bool("file_reader", "block_when_full", config->block_when_full);
    config->log_progress = get_config_bool("file_reader", "log_progress", config->log_progress);
    config->progress_interval_ms = get_config_int("file_reader", "progress_interval_ms", config->progress_interval_ms);

    return PLATFORM_ERROR_SUCCESS;
}

void* file_reader_thread_function(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    if (!thread_config || !thread_config->data) {
        logger_log(LOG_ERROR, "Invalid file_reader thread arguments");
        return NULL;
    }

    FileReaderConfig* config = (FileReaderConfig*)thread_config->data;
    if (!config || !config->filepath || !config->foreign_thread_label) {
        logger_log(LOG_ERROR, "Invalid file reader configuration");
        return NULL;
    }

    thread_reader_init_config(config);
    logger_log(LOG_INFO, "File reader thread started");

    PlatformErrorCode error_code;
    PlatformFileHandle file = open_file(config->filepath, &error_code);
    if (!file) {
        char error_buffer[256];
        platform_get_error_message_from_code(error_code, error_buffer, sizeof(error_buffer));
        logger_log(LOG_ERROR, "Failed to open file '%s': %s",
            config->filepath, error_code != PLATFORM_ERROR_SUCCESS ? error_buffer : "Unknown error");
        return (void*)THREAD_ERROR_FILE_OPEN;
    }

    uint64_t file_size;
    error_code = platform_file_get_size(file, &file_size);
    if (error_code != PLATFORM_ERROR_SUCCESS) {
        char error_buffer[256];
        platform_get_error_message_from_code(error_code, error_buffer, sizeof(error_buffer));
        logger_log(LOG_ERROR, "Failed to get file size for '%s': %s",
            config->filepath, error_buffer);
        platform_file_close(file);
        return (void*)THREAD_ERROR_FILE_READ;
    }

    // Use configured chunk size or default to MESSAGE_CONTENT_SIZE
    size_t chunk_size = config->chunk_size > 0 ?
        config->chunk_size : MESSAGE_CONTENT_SIZE;

    // Read and send file contents in chunks
    size_t bytes_read;
    size_t total_bytes = 0;
    uint32_t last_progress = get_time_ms();

    while (!shutdown_signalled()) {
        uint8_t buffer[MESSAGE_CONTENT_SIZE];
        error_code = platform_file_read(file, buffer, chunk_size, &bytes_read);
        if (error_code != PLATFORM_ERROR_SUCCESS) {
            char error_buffer[256];
            platform_get_error_message_from_code(error_code, error_buffer, sizeof(error_buffer));
            logger_log(LOG_ERROR, "Failed to read from file '%s': %s",
                config->filepath, error_buffer);
            platform_file_close(file);
            return (void*)THREAD_ERROR_FILE_READ;
        }

        if (bytes_read == 0) {
            break;  // End of file
        }

        // Create message on stack and copy into queue's pre-allocated buffer
        Message_T message = { 0 };
        message.header.type = MSG_TYPE_FILE_CHUNK;
        message.header.content_size = bytes_read;

        if (message.header.content_size > MESSAGE_CONTENT_SIZE) {
            logger_log(LOG_ERROR, "Chunk size exceeds message content buffer");
            platform_file_close(file);
            return (void*)THREAD_ERROR_BUFFER_OVERFLOW;
        }

        memcpy(message.content, buffer, bytes_read);

        ThreadRegistryError send_result = push_message(
            config->foreign_thread_label,
            &message,
            config->queue_timeout_ms
        );

        if (send_result != THREAD_REG_SUCCESS) {
            platform_file_close(file);
            return (void*)THREAD_ERROR_QUEUE_FULL;
        }

        total_bytes += bytes_read;

        // Log progress if enabled
        if (config->log_progress) {
            uint32_t now = get_time_ms();
            if (now - last_progress >= config->progress_interval_ms) {
                logger_log(LOG_INFO, "Read %zu of %zu bytes (%.1f%%)",
                    total_bytes, (size_t)file_size,
                    (float)total_bytes * 100 / file_size);
                last_progress = now;
            }
        }

        if (config->chunk_delay_ms > 0) {
            sleep_ms(config->chunk_delay_ms);
        }
    }

    platform_file_close(file);

    // Handle different read modes
    if (config->read_mode == FILE_READ_LOOP) {
        sleep_ms(config->reload_delay_ms);
        return (void*)THREAD_SUCCESS;  // Will cause processor to be called again
    }

    logger_log(LOG_INFO, "File reader thread shutting down");
    return (void*)THREAD_SUCCESS;
}

//     size_t file_size;
//     if (!get_file_size(file, &file_size)) {
//         logger_log(LOG_ERROR, "Failed to get file size for '%s'", config->filepath);
//         close_file(file);
//         return THREAD_ERROR_FILE_READ;
//     }

//     // Use configured chunk size or default to MAX_MESSAGE_SIZE
//     size_t chunk_size = config->chunk_size > 0 ? 
//                        config->chunk_size : MAX_MESSAGE_SIZE;

//     // Read and send file contents in chunks
//     size_t bytes_read;
//     size_t total_bytes = 0;
//     uint32_t last_progress = get_time_ms();

//     while (!shutdown_signalled()) {
//         uint8_t buffer[MAX_MESSAGE_SIZE];
//         if (!read_file_chunk(file, buffer, chunk_size, &bytes_read, &error_msg)) {
//             logger_log(LOG_ERROR, "Failed to read from file '%s': %s", 
//                       config->filepath, error_msg);
//             close_file(file);
//             return THREAD_ERROR_FILE_READ;
//         }

//         if (bytes_read == 0) {
//             break;  // End of file
//         }

//         // Create and send message
//         Message msg = {
//             .type = MESSAGE_TYPE_FILE_DATA,
//             .data = malloc(bytes_read),
//             .size = bytes_read
//         };

//         if (!msg.data) {
//             close_file(file);
//             return THREAD_ERROR_OUT_OF_MEMORY;
//         }

//         memcpy(msg.data, buffer, bytes_read);

//         ThreadRegistryError send_result = send_message(config->target_thread_label, 
//                                                      &msg, 
//                                                      config->queue_timeout_ms);

//         if (send_result != THREAD_REG_SUCCESS) {
//             free(msg.data);
//             close_file(file);
//             return THREAD_ERROR_QUEUE_FULL;
//         }

//         total_bytes += bytes_read;

//         // Log progress if enabled
//         if (config->log_progress) {
//             uint32_t now = get_time_ms();
//             if (now - last_progress >= config->progress_interval_ms) {
//                 float progress = (float)total_bytes / file_size * 100.0f;
//                 logger_log(LOG_INFO, "File read progress: %.1f%% (%zu/%zu bytes)", 
//                           progress, total_bytes, file_size);
//                 last_progress = now;
//             }
//         }

//         // Apply configured delay between chunks
//         if (config->chunk_delay_ms > 0) {
//             sleep_ms(config->chunk_delay_ms);
//         }
//     }

//     close_file(file);

//     // Handle different read modes
//     if (config->read_mode == FILE_READ_LOOP) {
//         sleep_ms(config->reload_delay_ms);
//         return THREAD_SUCCESS;  // Will cause processor to be called again
//     }

//     return THREAD_SUCCESS;
// }

// void get_file_reader_config(FileReaderConfig* config) {
//     if (!config) return;

//     // Start with defaults
//     *config = DefaultFileReaderConfig;

//     // Read mode
//     const char* mode_str = get_config_string("file_reader", "read_mode", "once");
//     if (strcmp_nocase(mode_str, "loop") == 0) {
//         config->read_mode = FILE_READ_LOOP;
//     } else if (strcmp_nocase(mode_str, "watch") == 0) {
//         config->read_mode = FILE_READ_WATCH;
//     } else {
//         config->read_mode = FILE_READ_ONCE;
//     }

//     // Chunk and timing settings
//     config->chunk_size = (uint32_t)get_config_int("file_reader", "chunk_size", 0);
//     config->chunk_delay_ms = (uint32_t)get_config_int("file_reader", "chunk_delay_ms", 0);
//     config->reload_delay_ms = (uint32_t)get_config_int("file_reader", "reload_delay_ms", 1000);

//     // Queue settings
//     config->queue_timeout_ms = (uint32_t)get_config_int("file_reader", "queue_timeout_ms", 
//                                                        DEFAULT_THREAD_WAIT_TIMEOUT_MS);
//     config->max_queue_size = (uint32_t)get_config_int("file_reader", "max_queue_size", 0);
//     config->block_when_full = get_config_bool("file_reader", "block_when_full", true);

//     // Monitoring settings
//     config->log_progress = get_config_bool("file_reader", "log_progress", true);
//     config->progress_interval_ms = (uint32_t)get_config_int("file_reader", "progress_interval_ms", 1000);
// }

// // Helper function to start a file reader with config-file based configuration
// bool start_file_reader(const char* filepath, const char* target_send_thread_label) {
//     FileReaderConfig config;
//     get_file_reader_config(&config);

//     ThreadConfig* thread_config = create_file_reader_thread(filepath, target_send_thread_label, &config);
//     if (!thread_config) {
//         logger_log(LOG_ERROR, "Failed to create file reader thread configuration");
//         return false;
//     }

//     if (!app_thread_create(thread_config)) {
//         logger_log(LOG_ERROR, "Failed to create file reader thread");
//         // Clean up allocated memory
//         free((void*)((FileReaderConfig*)thread_config->data)->filepath);
//         free((void*)((FileReaderConfig*)thread_config->data)->target_thread_label);
//         free(thread_config->data);
//         free((void*)thread_config->label);
//         free(thread_config);
//         return false;
//     }

//     return true;
// }
