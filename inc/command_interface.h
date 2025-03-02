/*
*
*/
#ifndef COMMAND_INTERFACE_H
#define COMMAND_INTERFACE_H

/**
 * @brief Structure to hold
 */
typedef struct command_interface_thread_args_T {
    void* data;
    int port;
} command_interface_thread_args_T;


void* command_interface_thread_function(void* arg);

#endif // COMMAND_INTERFACE_H
