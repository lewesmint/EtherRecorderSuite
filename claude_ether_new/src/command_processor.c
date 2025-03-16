#include "command_processor.h"

#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "platform_string.h"
#include "logger.h"
#include "utils.h"



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

static char* trim_whitespace(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void process_log_level_command(const char* value) {
    bool found = false;
    size_t table_size = sizeof(log_level_table) / sizeof(log_level_table[0]);

    for (size_t i = 0; i < table_size; i++) {        
        if (strcmp_nocase(value, log_level_table[i].name) == 0) {
            LogLevel previous_level = logger_get_level();
            logger_log(previous_level, "Log level changing from %s to %s", 
                      get_level_name(previous_level), log_level_table[i].name);
            
            logger_set_level(log_level_table[i].level);
            logger_log(log_level_table[i].level, "Log level changed to %s", 
                      log_level_table[i].name);
            found = true;
            break;
        }
    }
    if (!found) {
        logger_log(LOG_WARN, "Unknown log level: %s", value);
    }
}

void process_command(const char* command) {
    if (!command) {
        return;
    }

    char cmd_buf[256];
    strncpy(cmd_buf, command, sizeof(cmd_buf));
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';

    char* trimmed = trim_whitespace(cmd_buf);
    char* equal_sign = strchr(trimmed, '=');
    
    if (equal_sign) {
        *equal_sign = '\0';
        char* left = trim_whitespace(trimmed);
        char* right = trim_whitespace(equal_sign + 1);

        if (strcmp_nocase(left, "log_level") == 0) {
            process_log_level_command(right);
            return;
        }
    }

    if (strcmp(trimmed, "SOME_COMMAND") == 0) {
        logger_log(LOG_INFO, "Processing SOME_COMMAND");
    }
    else {
        logger_log(LOG_WARN, "Unknown command: %s", trimmed);
    }
}
