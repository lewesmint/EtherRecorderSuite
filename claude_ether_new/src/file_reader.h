#ifndef FILE_READER_H
#define FILE_READER_H

#include <stdbool.h>
#include <stdint.h>
#include "app_thread.h"
#include "platform_threads.h"

typedef enum {
    FILE_READ_ONCE,   ///< Read file once and exit
    FILE_READ_LOOP,   ///< Read file repeatedly
    FILE_READ_WATCH   ///< Watch file for changes and read
} FileReadMode;

typedef struct {
    FileReadMode read_mode;         ///< Reading mode
    uint32_t chunk_size;            ///< Size of each chunk to read
    uint32_t chunk_delay_ms;        ///< Delay between chunks
    uint32_t reload_delay_ms;       ///< Delay before reloading in loop mode
    uint32_t queue_timeout_ms;      ///< Timeout for queue operations
    uint32_t max_queue_size;        ///< Maximum queue size
    bool block_when_full;           ///< Whether to block when queue is full
    bool log_progress;              ///< Whether to log progress
    uint32_t progress_interval_ms;  ///< Progress logging interval
} FileReaderConfig;

// Function declarations
ThreadConfig* create_file_reader_thread(const char* filepath, 
                                      const char* target_thread_label,
                                      const FileReaderConfig* config);

void get_file_reader_config(FileReaderConfig* config);
bool start_file_reader(const char* filepath, const char* target_send_thread_label);

#endif // FILE_READER_H
