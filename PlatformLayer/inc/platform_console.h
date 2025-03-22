/**
 * @file platform_console.h
 * @brief Platform-agnostic console/terminal operations
 */
#ifndef PLATFORM_CONSOLE_H
#define PLATFORM_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Console text colors
 */
typedef enum {
    CONSOLE_COLOR_DEFAULT = 0,
    CONSOLE_COLOR_BLACK,
    CONSOLE_COLOR_RED,
    CONSOLE_COLOR_GREEN,
    CONSOLE_COLOR_YELLOW,
    CONSOLE_COLOR_BLUE,
    CONSOLE_COLOR_MAGENTA,
    CONSOLE_COLOR_CYAN,
    CONSOLE_COLOR_WHITE
} ConsoleColor;

/**
 * @brief Console text attributes
 */
typedef enum {
    CONSOLE_ATTR_NORMAL    = 0x00,
    CONSOLE_ATTR_BOLD      = 0x01,
    CONSOLE_ATTR_DIM       = 0x02,
    CONSOLE_ATTR_ITALIC    = 0x04,
    CONSOLE_ATTR_UNDERLINE = 0x08,
    CONSOLE_ATTR_BLINK     = 0x10,
    CONSOLE_ATTR_REVERSE   = 0x20,
    CONSOLE_ATTR_HIDDEN    = 0x40
} ConsoleAttribute;

/**
 * @brief Initialize the console subsystem
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_init(void);

/**
 * @brief Clean up the console subsystem
 */
void platform_console_cleanup(void);

/**
 * @brief Enable or disable virtual terminal processing
 * @param enable true to enable, false to disable
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_enable_vt_mode(bool enable);

/**
 * @brief Enable or disable console input echo
 * @param enable true to enable, false to disable
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_set_echo(bool enable);

/**
 * @brief Enable or disable console input line buffering
 * @param enable true to enable, false to disable
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_set_line_buffering(bool enable);

/**
 * @brief Enable or disable console quick edit mode (Windows-specific)
 * @param enable true to enable, false to disable
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_set_quick_edit(bool enable);

/**
 * @brief Set console text color
 * @param color The color to set
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_set_color(ConsoleColor color);

/**
 * @brief Set console text attributes
 * @param attrs Combination of ConsoleAttribute flags
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_set_attributes(uint32_t attrs);

/**
 * @brief Reset console text color and attributes to default
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_reset_formatting(void);

/**
 * @brief Clear the console screen
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_clear(void);

/**
 * @brief Get console window size
 * @param[out] width Width in characters
 * @param[out] height Height in characters
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_get_size(int* width, int* height);

/**
 * @brief Set console cursor position
 * @param x Column position (0-based)
 * @param y Row position (0-based)
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_set_cursor(int x, int y);

/**
 * @brief Show or hide the console cursor
 * @param visible true to show, false to hide
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_show_cursor(bool visible);

/**
 * @brief Check if the console supports ANSI escape sequences
 * @return true if ANSI sequences are supported
 */
bool platform_console_supports_ansi(void);

/**
 * @brief Read a single character from the console (non-blocking)
 * @param[out] ch The character read
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_console_read_char(char* ch);

/**
 * @brief Check if a key is available to be read
 * @return true if a key is available
 */
bool platform_console_key_available(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_CONSOLE_H
