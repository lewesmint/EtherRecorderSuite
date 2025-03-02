/**
 * @file config.h
 * @brief Configuration system for loading and retrieving configuration values.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define CONFIG_MAX_VALUE_LENGTH 200

/**
 * @brief Loads the configuration from a file.
 *
 * @param filename The name of the configuration file.
 * @param log_result The result of the configuration load.
 * @return true if the configuration was loaded successfully, false otherwise.
 */
bool load_config(const char *filename, char* log_result);

/**
 * @brief Retrieves a string value from the configuration.
 *
 * @param section The section of the configuration.
 * @param key The key within the section.
 * @param default_value The default value if the key is not found.
 * @return The string value associated with the key.
 */
const char *get_config_string(const char *section, const char *key, const char *default_value);

/**
 * @brief Retrieves an integer value from the configuration.
 *
 * @param section The section of the configuration.
 * @param key The key within the section.
 * @param default_value The default value if the key is not found.
 * @return The integer value associated with the key.
 */
int get_config_int(const char *section, const char *key, int default_value);

/**
 * @brief Retrieves a boolean value from the configuration.
 *
 * @param section The section of the configuration.
 * @param key The key within the section.
 * @param default_value The default value if the key is not found.
 * @return The boolean value associated with the key.
 */
bool get_config_bool(const char *section, const char *key, bool default_value);

/**
 * @brief Retrieves a floating point value from the configuration.
 *
 * @param section The section of the configuration.
 * @param key The key within the section.
 * @param default_value The default value if the key is not found.
 * @return The floating point value associated with the key.
 */
double get_config_float(const char *section, const char *key, double default_value);

/**
 * @brief Retrieves a hexadecimal value from the configuration.
 *
 * @param section The section of the configuration.
 * @param key The key within the section.
 * @param default_value The default value if the key is not found.
 * @return The hexadecimal value associated with the key.
 */
uint64_t get_config_hex(const char *section, const char *key, uint64_t default_value);

/**
 * @brief Retrieves a 16-bit unsigned integer value from the configuration.
 *
 * @param section The section of the configuration.
 * @param key The key within the section.
 * @param default_value The default value if the key is not found.
 * @return The 16-bit unsigned integer value associated with the key.
 */
uint16_t get_config_uint16(const char* section, const char* key, uint16_t default_value);

/**
 * @brief Frees the memory allocated for the configuration entries.
 */
void free_config(void);

#endif // APP_CCONFIG_H
