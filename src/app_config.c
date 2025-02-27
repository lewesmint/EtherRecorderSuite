#include "app_config.h"
#include "platform_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "logger.h"

#define MAX_LINE_LENGTH 256
#define MAX_SECTION_LENGTH 50
#define MAX_KEY_LENGTH 50
#define CONFIG_MAX_VALUE_LENGTH 200

typedef struct ConfigEntry {
    char section[MAX_SECTION_LENGTH];
    char key[MAX_KEY_LENGTH];
    char value[CONFIG_MAX_VALUE_LENGTH];
    struct ConfigEntry *next;
} ConfigEntry;

static ConfigEntry *config_entries = NULL;

/**
 * @brief Trim leading and trailing whitespace from a string in place.
 * @param str The string to be trimmed.
 */
static void trim_whitespace(char* str) {
    char* start = str;
    char* end;

    if (!str || *str == '\0') return;

    // Trim leading space
    while (isspace((unsigned char)*start)) start++;

    // If the string was entirely spaces, return an empty string
    if (*start == '\0') {
        *str = '\0';
        return;
    }

    // Trim trailing space
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = '\0';

    // Move the trimmed content to the beginning of str
    if (start != str) {
        memmove(str, start, end - start + 2);  // Include null terminator
    }
}


static ConfigEntry* find_config_entry(const char *section, const char *key) {
    ConfigEntry *entry = config_entries;
    while (entry) {
        if (str_cmp_nocase(entry->section, section) == 0 &&
            str_cmp_nocase(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * @brief Removes comments (`;` or `#`) from a line, while preserving quoted values.
 * @param str The input line.
 */
static void trim_comments(char* str) {
    bool in_quotes = false;
    char* p = str;

    while (*p) {
        if (*p == '"' || *p == '\'') {
            in_quotes = !in_quotes; // Toggle quote state
        }
        else if (!in_quotes && (*p == ';' || *p == '#')) {
            *p = '\0';  // Null-terminate to remove comment
            break;
        }
        p++;
    }
    trim_whitespace(str); // Ensure no trailing spaces remain
}


/**
 * @copydoc load_config
 */
bool load_config(const char* filename, char* log_result) {
    char full_path[MAX_PATH];
    if (!resolve_full_path(filename, full_path, sizeof(full_path))) {
        snprintf(log_result, LOG_MSG_BUFFER_SIZE, "Failed to resolve full path for: %s\n", filename);
        return false;
    }

    printf("Looking for config with path: %s\n", full_path);
    FILE* file = fopen(full_path, "r");
    if (!file) {
        snprintf(log_result, LOG_MSG_BUFFER_SIZE,
            "Failed to load configuration file: %s\n"
            "Default settings will be used\n", full_path);
        return false;
    }
    else {
        snprintf(log_result, LOG_MSG_BUFFER_SIZE, "Loading configuration file: %s\n", full_path);
    }

    char line[MAX_LINE_LENGTH];
    char current_section[MAX_SECTION_LENGTH] = "";

    while (fgets(line, sizeof(line), file)) {
        trim_whitespace(line);
        trim_comments(line);  // Remove inline comments

        if (line[0] == '\0') continue; // Skip empty lines

        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, line + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
            }
        }
        else {
            char* equals = strchr(line, '=');
            if (equals) {
                *equals = '\0';
                char* key = line;
                char* value = equals + 1;
                trim_whitespace(key);
                trim_whitespace(value);

                ConfigEntry* entry = (ConfigEntry*)malloc(sizeof(ConfigEntry));
                strncpy(entry->section, current_section, sizeof(entry->section) - 1);
                strncpy(entry->key, key, sizeof(entry->key) - 1);
                strncpy(entry->value, value, sizeof(entry->value) - 1);
                entry->section[sizeof(entry->section) - 1] = '\0';
                entry->key[sizeof(entry->key) - 1] = '\0';
                entry->value[sizeof(entry->value) - 1] = '\0';
                entry->next = config_entries;
                config_entries = entry;

                // Log the stored configuration entry
                // printf("Stored config entry: Section [%s], Key [%s], Value [%s]\n", entry->section, entry->key, entry->value);
            }
        }
    }

    fclose(file);
    return true; // Indicate success
}


/**
 * @brief Retrieves a configuration value as a string.
 * @param section The section in the configuration file.
 * @param key The key within the section.
 * @param default_value The default value to return if the key is not found.
 * @return The configuration value as a string, or @p default_value if not found.
 */
const char* get_config_string(const char *section, const char *key, const char *default_value) {
    ConfigEntry *entry = find_config_entry(section, key);
    return entry ? entry->value : default_value;
}

/**
 * @copydoc get_config_int
 */
int get_config_int(const char *section, const char *key, int default_value) {
    const char *value = get_config_string(section, key, NULL);
    return value ? atoi(value) : default_value;
}

/**
 * @copydoc get_config_bool
 */
bool get_config_bool(const char* section, const char* key, bool default_value) {
    const char* value = get_config_string(section, key, NULL);
    if (!value) return default_value;

    /* Check for truthy values */
    if (str_cmp_nocase(value, "true") == 0 ||
        str_cmp_nocase(value, "yes") == 0 ||
        str_cmp_nocase(value, "on") == 0 ||
        (value[0] == '1' && value[1] == '\0')) {
        return true;
    }

    /* Check for falsy values */
    if (str_cmp_nocase(value, "false") == 0 ||
        str_cmp_nocase(value, "no") == 0 ||
        str_cmp_nocase(value, "off") == 0 ||
        (value[0] == '0' && value[1] == '\0')) {
        return false;
    }

    /* Unrecognized values default */
    return default_value;
}

/**
 * @copydoc get_config_hex
 */
uint64_t get_config_hex(const char *section, const char *key, uint64_t default_value) {
    const char *value = get_config_string(section, key, NULL);
    return value ? strtoull(value, NULL, 16) : default_value;
}

/**
 * @copydoc free_config
 */
void free_config() {
    ConfigEntry *entry = config_entries;
    while (entry) {
        ConfigEntry *next = entry->next;
        free(entry);
        entry = next;
    }
    config_entries = NULL;
}
