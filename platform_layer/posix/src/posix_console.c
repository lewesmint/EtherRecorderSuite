/**
 * @file platform_console.c
 * @brief POSIX implementation of platform console operations
 */
#include "../../inc/platform_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include "platform_error.h"

// Store original terminal settings
static struct termios g_original_term;
static bool g_is_initialized = false;

PlatformErrorCode platform_console_init(void) {
    if (g_is_initialized) {
        return PLATFORM_ERROR_SUCCESS;
    }

    // Save current terminal settings
    if (tcgetattr(STDIN_FILENO, &g_original_term) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    g_is_initialized = true;
    return PLATFORM_ERROR_SUCCESS;
}

void platform_console_cleanup(void) {
    if (g_is_initialized) {
        // Restore original terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &g_original_term);
        g_is_initialized = false;
    }
}

PlatformErrorCode platform_console_enable_vt_mode(bool enable) {
    // Most POSIX terminals support VT sequences by default
    (void)enable; // Unused parameter
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_echo(bool enable) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    struct termios term;
    if (tcgetattr(STDIN_FILENO, &term) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    if (enable) {
        term.c_lflag |= ECHO;
    } else {
        term.c_lflag &= ~ECHO;
    }

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_line_buffering(bool enable) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    struct termios term;
    if (tcgetattr(STDIN_FILENO, &term) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    if (enable) {
        term.c_lflag |= ICANON;
    } else {
        term.c_lflag &= ~ICANON;
        // Set min characters and timeout
        term.c_cc[VMIN] = 1;
        term.c_cc[VTIME] = 0;
    }

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_quick_edit(bool enable) {
    // Quick edit is a Windows-specific feature
    (void)enable; // Unused parameter
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_color(ConsoleColor color) {
    if (!platform_console_supports_ansi()) {
        return PLATFORM_ERROR_NOT_SUPPORTED;
    }

    const char* color_code;
    switch (color) {
        case CONSOLE_COLOR_DEFAULT: color_code = "0"; break;
        case CONSOLE_COLOR_BLACK:   color_code = "30"; break;
        case CONSOLE_COLOR_RED:     color_code = "31"; break;
        case CONSOLE_COLOR_GREEN:   color_code = "32"; break;
        case CONSOLE_COLOR_YELLOW:  color_code = "33"; break;
        case CONSOLE_COLOR_BLUE:    color_code = "34"; break;
        case CONSOLE_COLOR_MAGENTA: color_code = "35"; break;
        case CONSOLE_COLOR_CYAN:    color_code = "36"; break;
        case CONSOLE_COLOR_WHITE:   color_code = "37"; break;
        default: return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    printf("\033[%sm", color_code);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_attributes(uint32_t attrs) {
    if (!platform_console_supports_ansi()) {
        return PLATFORM_ERROR_NOT_SUPPORTED;
    }

    // Reset first
    printf("\033[0m");

    // Apply each attribute
    if (attrs & CONSOLE_ATTR_BOLD)      printf("\033[1m");
    if (attrs & CONSOLE_ATTR_DIM)       printf("\033[2m");
    if (attrs & CONSOLE_ATTR_ITALIC)    printf("\033[3m");
    if (attrs & CONSOLE_ATTR_UNDERLINE) printf("\033[4m");
    if (attrs & CONSOLE_ATTR_BLINK)     printf("\033[5m");
    if (attrs & CONSOLE_ATTR_REVERSE)   printf("\033[7m");
    if (attrs & CONSOLE_ATTR_HIDDEN)    printf("\033[8m");

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_reset_formatting(void) {
    if (!platform_console_supports_ansi()) {
        return PLATFORM_ERROR_NOT_SUPPORTED;
    }

    printf("\033[0m");
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_clear(void) {
    if (!platform_console_supports_ansi()) {
        return PLATFORM_ERROR_NOT_SUPPORTED;
    }

    printf("\033[2J\033[H");
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_get_size(int* width, int* height) {
    if (!width || !height) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    *width = ws.ws_col;
    *height = ws.ws_row;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_cursor(int x, int y) {
    if (!platform_console_supports_ansi()) {
        return PLATFORM_ERROR_NOT_SUPPORTED;
    }

    printf("\033[%d;%dH", y + 1, x + 1);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_show_cursor(bool visible) {
    if (!platform_console_supports_ansi()) {
        return PLATFORM_ERROR_NOT_SUPPORTED;
    }

    printf("\033[?25%c", visible ? 'h' : 'l');
    return PLATFORM_ERROR_SUCCESS;
}

bool platform_console_supports_ansi(void) {
    static bool checked = false;
    static bool supported = false;

    if (!checked) {
        const char* term = getenv("TERM");
        if (term && (
            strcmp(term, "xterm") == 0 ||
            strcmp(term, "xterm-256color") == 0 ||
            strcmp(term, "screen") == 0 ||
            strcmp(term, "linux") == 0 ||
            strstr(term, "color") != NULL
        )) {
            supported = true;
        }
        checked = true;
    }

    return supported;
}

PlatformErrorCode platform_console_read_char(char* ch) {
    if (!ch) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    // Temporarily disable line buffering
    struct termios term;
    if (tcgetattr(STDIN_FILENO, &term) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    struct termios new_term = term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 0;
    new_term.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    // Read a single character
    ssize_t result = read(STDIN_FILENO, ch, 1);

    // Restore original settings
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    if (result < 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return (result == 1) ? PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_TIMEOUT;
}

bool platform_console_key_available(void) {
    if (!g_is_initialized) {
        return false;
    }

    int bytes_available;
    if (ioctl(STDIN_FILENO, FIONREAD, &bytes_available) != 0) {
        return false;
    }

    return bytes_available > 0;
}
