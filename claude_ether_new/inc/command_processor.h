/**
 * @file command_processor.h
 * @brief Command processing functionality for the command interface
 */
#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <stddef.h>

/**
 * @brief Process a received command string
 * @param command The command string to process
 */
void process_command(const char* command);

#endif // COMMAND_PROCESSOR_H
