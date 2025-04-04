#ifndef SHUTDOWN_HANDLER_H
#define SHUTDOWN_HANDLER_H

#include <stdbool.h>

/**
 * @brief Installs the shutdown handler.
 *
 * This function sets up the signal handler to catch Ctrl-C (SIGINT)
 * and properly trigger the shutdown sequence.
 */
bool install_shutdown_handler(void);

/**
 * @brief Signals that a shutdown should begin.
 *
 * This sets the shutdown flag atomically, signaling all threads to exit.
 */
void signal_shutdown(void);

/**
 * @brief Checks whether a shutdown has been signaled.
 *
 * This function ensures an atomic read of the shutdown flag.
 * @return true if shutdown is signaled, false otherwise.
 */
bool shutdown_signalled(void);

/**
 * @brief Blocks until the shutdown event is triggered or the timeout expires.
 *
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return true if shutdown was signaled, false if timeout occurred
 */
bool wait_for_shutdown_event(int timeout_ms);

/**
 * @brief Cleans up the shutdown handler resources.
 */
void cleanup_shutdown_handler(void);

#endif // SHUTDOWN_HANDLER_H
