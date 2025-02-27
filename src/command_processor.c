// command_processor.c
#include "command_processor.h"

#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "logger.h"
#include "platform_utils.h"


extern void logger_set_level(LogLevel level);


/* A lookup table mapping log level strings to their enum values */
typedef struct {
    const char* name;
    LogLevel level;
} LogLevelMap;


static const LogLevelMap log_level_table[] = {
    {"trace",    LOG_TRACE},
    {"debug",    LOG_DEBUG},
    {"info",     LOG_INFO},
    {"notice",   LOG_NOTICE},
    {"warn",     LOG_WARN},
    {"warning",  LOG_WARN},
    {"error",    LOG_ERROR},
    {"critical", LOG_CRITICAL},
    {"fatal",    LOG_FATAL},
};

/**
 * @brief Trim leading and trailing whitespace from a string.
 *
 * This function modifies the provided string in place.
 *
 * @param str A modifiable C string.
 * @return A pointer to the trimmed string.
 */
static char* trim_whitespace(char* str)
{
    char* end;

    /* Trim leading whitespace */
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {  /* All spaces? */
        return str;
    }

    /* Trim trailing whitespace */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';

    return str;
}

/**
 * @brief Process a log level command.
 *
 * This function attempts to match the provided log level string (value) with the
 * known log levels. If a match is found, it sets the log level using logger_set_level()
 * and logs an informational message. If not, it logs a warning.
 *
 * @param value The string representing the log level.
 */
static void process_log_level_command(const char* value)
{
    bool found = false;
    size_t table_size = sizeof(log_level_table) / sizeof(log_level_table[0]);

    for (size_t i = 0; i < table_size; i++) {        
        if (str_cmp_nocase(value, log_level_table[i].name) == 0) {
            logger_set_level(log_level_table[i].level);
            // TODO 
			// should check whether the log will now show INFO or not
            logger_log(LOG_INFO, "Log level changed to %s", log_level_table[i].name);
            found = true;
            break;
        }
    }
    if (!found) {
        logger_log(LOG_WARN, "Unknown log level: %s", value);
    }
}

/**
 * @brief Process a command string.
 *
 * This function handles commands that change the log level and tolerates extra spaces
 * before, after and around the '=' character. For example, a command like:
 *
 *     "   log_level  =   debug  "
 *
 * will be correctly interpreted. Other commands TBD ...
 *
 * @param command The command string.
 */
void process_command(const char* command)
{
    /* Copy the command into a local buffer to allow modification */
    char cmd_buf[256];
    strncpy(cmd_buf, command, sizeof(cmd_buf));
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';

    /* Trim leading and trailing whitespace */
    char* trimmed = trim_whitespace(cmd_buf);

    /* Look for an '=' character, which indicates an assignment command */
    char* equal_sign = strchr(trimmed, '=');
    if (equal_sign != NULL) {
        /* Split into left and right parts and trim each side */
        *equal_sign = '\0';
        char* left = trim_whitespace(trimmed);
        char* right = trim_whitespace(equal_sign + 1);

        /* Check for the log_level command (case-insensitive) */
        if (str_cmp_nocase(left, "log_level") == 0) {
            process_log_level_command(right);
            return;
        }
    }

    /* Process other commands */
    if (strcmp(trimmed, "SOME_COMMAND") == 0) {
        logger_log(LOG_INFO, "Processing SOME_COMMAND");
        /* Execute the specific action for SOME_COMMAND */
    }
    else {
        logger_log(LOG_WARN, "Unknown command: %s", trimmed);
    }
}

