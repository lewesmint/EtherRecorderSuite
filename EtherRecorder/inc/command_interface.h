#ifndef COMMAND_INTERFACE_H
#define COMMAND_INTERFACE_H

#include "app_thread.h"

/**
 * @brief Get the command interface thread configuration
 * @return Pointer to the thread configuration
 */
ThreadConfig* get_command_interface_thread(void);

#endif // COMMAND_INTERFACE_H
