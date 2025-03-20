#include "file_reader.h"
#include "logger.h"
#include "platform_threads.h"
#include "platform_string.h"
#include "platform_error.h"
#include "message_types.h"
#include "app_config.h"
#include "thread_registry.h"
#include "shutdown_handler.h"
#include "utils.h"


typedef struct PlatformFile* FileHandle;

static FileHandle open_file(const char* filepath, const char** error_msg) {
    // Temporary implementation using standard C file operations
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        *error_msg = "Failed to open file";
        return NULL;
    }
    return (FileHandle)file;
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

// Default configuration template
const ThreadConfig FileReaderThreadTemplate = {
    .label = NULL,                    // Must be set per instance
    .func = NULL,                     // Not used - we use msg_processor
    .data = NULL,                     // Will hold FileReaderConfig
    .thread_id = 0,                   // Set by registry
    .pre_create_func = NULL,          // Use defaults
    .post_create_func = NULL,
    .init_func = NULL,
    .exit_func = NULL,
    .suppressed = false,
    .msg_processor = file_reader_processor,
    .msg_batch_size = 1,              // Process one message at a time
    .queue_process_interval_ms = 0,   // Process as fast as possible
    .max_process_time_ms = 0          // No time limit
};

// Default file reader configuration
static const FileReaderConfig DefaultFileReaderConfig = {
    .filepath = NULL,                 // Must be set
    .target_queue_label = NULL,       // Must be set
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

ThreadConfig* create_file_reader_thread(const char* filepath, 
                                      const char* target_queue_label,
                                      const FileReaderConfig* config) {
    if (!filepath || !target_queue_label) {
        return NULL;
    }

    // Allocate and initialize configuration
    FileReaderConfig* reader_config = (FileReaderConfig*)malloc(sizeof(FileReaderConfig));
    if (!reader_config) {
        return NULL;
    }

    // Start with defaults, then override with provided config
    *reader_config = DefaultFileReaderConfig;
    if (config) {
        reader_config->read_mode = config->read_mode;
        reader_config->chunk_size = config->chunk_size;
        reader_config->chunk_delay_ms = config->chunk_delay_ms;
        reader_config->reload_delay_ms = config->reload_delay_ms;
        reader_config->queue_timeout_ms = config->queue_timeout_ms;
        reader_config->max_queue_size = config->max_queue_size;
        reader_config->block_when_full = config->block_when_full;
        reader_config->log_progress = config->log_progress;
        reader_config->progress_interval_ms = config->progress_interval_ms;
    }

    // Set required fields
    reader_config->filepath = strdup(filepath);
    reader_config->target_thread_label = strdup(target_queue_label);

    // Create thread configuration
    ThreadConfig* thread_config = (ThreadConfig*)malloc(sizeof(ThreadConfig));
    if (!thread_config) {
        free((void*)reader_config->filepath);
        free((void*)reader_config->target_thread_label);
        free(reader_config);
        return NULL;
    }

    // Start with template and customize
    *thread_config = FileReaderThreadTemplate;
    
    // Create unique label
    char* label = (char*)malloc(THREAD_LABEL_SIZE);
    if (!label) {
        free((void*)reader_config->filepath);
        free((void*)reader_config->target_thread_label);
        free(reader_config);
        free(thread_config);
        return NULL;
    }
    
    snprintf(label, THREAD_LABEL_SIZE, "FILE_READER_%s", 
             platform_basename(filepath));
    
    thread_config->label = label;
    thread_config->data = reader_config;

    return thread_config;
}

ThreadResult file_reader_processor(ThreadConfig* thread, Message* message) {
    FileReaderConfig* config = (FileReaderConfig*)thread->data;
    if (!config || !config->filepath || !config->target_thread_label) {
        return THREAD_ERROR_INVALID_CONFIG;
    }

    const char* error_msg = NULL;
    FileHandle file = open_file(config->filepath, &error_msg);
    if (!file) {
        logger_log(LOG_ERROR, "Failed to open file '%s': %s", 
                  config->filepath, error_msg);
        return THREAD_ERROR_FILE_OPEN;
    }

    size_t file_size;
    if (!get_file_size(file, &file_size)) {
        logger_log(LOG_ERROR, "Failed to get file size for '%s'", config->filepath);
        close_file(file);
        return THREAD_ERROR_FILE_READ;
    }

    // Use configured chunk size or default to MAX_MESSAGE_SIZE
    size_t chunk_size = config->chunk_size > 0 ? 
                       config->chunk_size : MAX_MESSAGE_SIZE;

    // Read and send file contents in chunks
    size_t bytes_read;
    size_t total_bytes = 0;
    uint32_t last_progress = get_time_ms();

    while (!shutdown_signalled()) {
        uint8_t buffer[MAX_MESSAGE_SIZE];
        if (!read_file_chunk(file, buffer, chunk_size, &bytes_read, &error_msg)) {
            logger_log(LOG_ERROR, "Failed to read from file '%s': %s", 
                      config->filepath, error_msg);
            close_file(file);
            return THREAD_ERROR_FILE_READ;
        }

        if (bytes_read == 0) {
            break;  // End of file
        }

        // Create and send message
        Message msg = {
            .type = MESSAGE_TYPE_FILE_DATA,
            .data = malloc(bytes_read),
            .size = bytes_read
        };
        
        if (!msg.data) {
            close_file(file);
            return THREAD_ERROR_OUT_OF_MEMORY;
        }
        
        memcpy(msg.data, buffer, bytes_read);
        
        ThreadRegistryError send_result = send_message(config->target_thread_label, 
                                                     &msg, 
                                                     config->queue_timeout_ms);
        
        if (send_result != THREAD_REG_SUCCESS) {
            free(msg.data);
            close_file(file);
            return THREAD_ERROR_QUEUE_FULL;
        }

        total_bytes += bytes_read;

        // Log progress if enabled
        if (config->log_progress) {
            uint32_t now = get_time_ms();
            if (now - last_progress >= config->progress_interval_ms) {
                float progress = (float)total_bytes / file_size * 100.0f;
                logger_log(LOG_INFO, "File read progress: %.1f%% (%zu/%zu bytes)", 
                          progress, total_bytes, file_size);
                last_progress = now;
            }
        }

        // Apply configured delay between chunks
        if (config->chunk_delay_ms > 0) {
            sleep_ms(config->chunk_delay_ms);
        }
    }

    close_file(file);

    // Handle different read modes
    if (config->read_mode == FILE_READ_LOOP) {
        sleep_ms(config->reload_delay_ms);
        return THREAD_SUCCESS;  // Will cause processor to be called again
    }
    
    return THREAD_SUCCESS;
}

void get_file_reader_config(FileReaderConfig* config) {
    if (!config) return;
    
    // Start with defaults
    *config = DefaultFileReaderConfig;
    
    // Read mode
    const char* mode_str = get_config_string("file_reader", "read_mode", "once");
    if (strcmp_nocase(mode_str, "loop") == 0) {
        config->read_mode = FILE_READ_LOOP;
    } else if (strcmp_nocase(mode_str, "watch") == 0) {
        config->read_mode = FILE_READ_WATCH;
    } else {
        config->read_mode = FILE_READ_ONCE;
    }
    
    // Chunk and timing settings
    config->chunk_size = (uint32_t)get_config_int("file_reader", "chunk_size", 0);
    config->chunk_delay_ms = (uint32_t)get_config_int("file_reader", "chunk_delay_ms", 0);
    config->reload_delay_ms = (uint32_t)get_config_int("file_reader", "reload_delay_ms", 1000);
    
    // Queue settings
    config->queue_timeout_ms = (uint32_t)get_config_int("file_reader", "queue_timeout_ms", 
                                                       DEFAULT_THREAD_WAIT_TIMEOUT_MS);
    config->max_queue_size = (uint32_t)get_config_int("file_reader", "max_queue_size", 0);
    config->block_when_full = get_config_bool("file_reader", "block_when_full", true);
    
    // Monitoring settings
    config->log_progress = get_config_bool("file_reader", "log_progress", true);
    config->progress_interval_ms = (uint32_t)get_config_int("file_reader", "progress_interval_ms", 1000);
}

// Helper function to start a file reader with config-file based configuration
bool start_file_reader(const char* filepath, const char* target_send_thread_label) {
    FileReaderConfig config;
    get_file_reader_config(&config);
    
    ThreadConfig* thread_config = create_file_reader_thread(filepath, target_send_thread_label, &config);
    if (!thread_config) {
        logger_log(LOG_ERROR, "Failed to create file reader thread configuration");
        return false;
    }
    
    if (!app_thread_create(thread_config)) {
        logger_log(LOG_ERROR, "Failed to create file reader thread");
        // Clean up allocated memory
        free((void*)((FileReaderConfig*)thread_config->data)->filepath);
        free((void*)((FileReaderConfig*)thread_config->data)->target_thread_label);
        free(thread_config->data);
        free((void*)thread_config->label);
        free(thread_config);
        return false;
    }
    
    return true;
}
