/**
* @file app_thread.h
* @brief Thread management functions.
*
*This file contains the functions to manage threads.
*
*/

#ifndef APP_THREAD_H
#define APP_THREAD_H

#include <winsock2.h>
#include <stdbool.h>

#include "platform_threads.h"
#include "logger.h"
#include "client_manager.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Function pointer type for pre-create functions.
 * @param arg Argument to the function.
 * @return Pointer to the result.
 */
typedef void* (*PreCreateFunc_T)(void* arg);

/**
 * @brief Function pointer type for post-create functions.
 * @param arg Argument to the function.
 * @return Pointer to the result.
 */
typedef void* (*PostCreateFunc_T)(void* arg);

/**
 * @brief Function pointer type for initialisation functions.
 * @param arg Argument to the function.
 * @return Pointer to the result.
 */
typedef void* (*InitFunc_T)(void* arg);

/**
 * @brief Function pointer type for exit functions.
 * @param arg Argument to the function.
 * @return Pointer to the result.
 */
typedef void* (*ExitFunc_T)(void* arg);

/**
 * @brief Function pointer type for thread functions.
 * @param arg Argument to the function.
 * @return Pointer to the result.
 */
typedef void* (*ThreadFunc_T)(void* arg);

/**
 * @brief Structure to hold thread arguments and functions.
 */
typedef struct AppThread_T {
    const char* label;                   ///< Label for the thread (e.g., "CLIENT" or "SERVER")
    ThreadFunc_T func;                   ///< Actual function to execute
    HANDLE thread_id;                    ///< Thread ID
    void *data;                          ///< Thread-specific data
    PreCreateFunc_T pre_create_func;     ///< Pre-create function
    PostCreateFunc_T post_create_func;   ///< Post-create function
    InitFunc_T init_func;                ///< Initialisation function
    ExitFunc_T exit_func;                ///< Exit function
    bool suppressed;                     ///< Suppress the thread
} AppThread_T;

/**
 * @brief Starts the threads based on the thread table.
 * @param threads The thread table.
 */
void start_threads(void);

/**
 * @brief Gets the label of the current thread.
 * @return The label of the current thread.
 */
const char* get_thread_label(void);

/**
 * @brief Sets the label of the current thread.
 * @param label The label to set.
 */
void set_thread_label(const char *label);

/**
 * @brief Waits for all other threads to complete.
 */
void wait_for_all_other_threads_to_complete(void);

/**
 * @brief Creates an application thread.
 * @param thread The thread arguments.
 */
void create_app_thread(AppThread_T *thread);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // APP_THREAD_H
