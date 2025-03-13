/**
 * @file win_console.c
 * @brief Windows implementation of platform console operations
 */
#include "../../inc/platform_console.h"

#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

#include "platform_error.h"

// Store original console settings
static DWORD g_original_mode = 0;
static HANDLE g_console_handle = INVALID_HANDLE_VALUE;
static bool g_is_initialized = false;

PlatformErrorCode platform_console_init(void) {
    if (g_is_initialized) {
        return PLATFORM_ERROR_SUCCESS;
    }

    g_console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_console_handle == INVALID_HANDLE_VALUE) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    // Save current console mode
    if (!GetConsoleMode(g_console_handle, &g_original_mode)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    g_is_initialized = true;
    return PLATFORM_ERROR_SUCCESS;
}

void platform_console_cleanup(void) {
    if (g_is_initialized && g_console_handle != INVALID_HANDLE_VALUE) {
        // Restore original console mode
        SetConsoleMode(g_console_handle, g_original_mode);
        g_is_initialized = false;
    }
}

PlatformErrorCode platform_console_enable_vt_mode(bool enable) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    DWORD mode;
    if (!GetConsoleMode(g_console_handle, &mode)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    if (enable) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    } else {
        mode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    }

    if (!SetConsoleMode(g_console_handle, mode)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_echo(bool enable) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    DWORD mode;
    if (!GetConsoleMode(hStdin, &mode)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    if (enable) {
        mode |= ENABLE_ECHO_INPUT;
    } else {
        mode &= ~ENABLE_ECHO_INPUT;
    }

    if (!SetConsoleMode(hStdin, mode)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_line_buffering(bool enable) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    DWORD mode;
    if (!GetConsoleMode(hStdin, &mode)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    if (enable) {
        mode |= ENABLE_LINE_INPUT;
    } else {
        mode &= ~ENABLE_LINE_INPUT;
    }

    if (!SetConsoleMode(hStdin, mode)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_quick_edit(bool enable) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    DWORD mode;
    if (!GetConsoleMode(hStdin, &mode)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    if (enable) {
        mode |= ENABLE_QUICK_EDIT_MODE;
    } else {
        mode &= ~ENABLE_QUICK_EDIT_MODE;
    }

    if (!SetConsoleMode(hStdin, mode)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_color(ConsoleColor color) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    WORD attr;
    switch (color) {
        case CONSOLE_COLOR_DEFAULT: attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        case CONSOLE_COLOR_BLACK:   attr = 0; break;
        case CONSOLE_COLOR_RED:     attr = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        case CONSOLE_COLOR_GREEN:   attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case CONSOLE_COLOR_YELLOW:  attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case CONSOLE_COLOR_BLUE:    attr = FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case CONSOLE_COLOR_MAGENTA: attr = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case CONSOLE_COLOR_CYAN:    attr = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case CONSOLE_COLOR_WHITE:   attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        default: return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!SetConsoleTextAttribute(g_console_handle, attr)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_attributes(uint32_t attrs) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    WORD win_attrs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Default color

    if (attrs & CONSOLE_ATTR_BOLD) {
        win_attrs |= FOREGROUND_INTENSITY;
    }
    if (attrs & CONSOLE_ATTR_REVERSE) {
        win_attrs = ((win_attrs & 0x0F) << 4) | ((win_attrs & 0xF0) >> 4);
    }
    // Note: Windows console doesn't support all ANSI attributes
    // Some are simulated or ignored

    if (!SetConsoleTextAttribute(g_console_handle, win_attrs)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_reset_formatting(void) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    WORD default_attrs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    if (!SetConsoleTextAttribute(g_console_handle, default_attrs)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_clear(void) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_console_handle, &csbi)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written;
    COORD home = {0, 0};

    // Fill with spaces
    if (!FillConsoleOutputCharacterW(g_console_handle, L' ', cells, home, &written)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    // Fill with current attributes
    if (!FillConsoleOutputAttribute(g_console_handle, csbi.wAttributes, cells, home, &written)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    // Move cursor to home
    if (!SetConsoleCursorPosition(g_console_handle, home)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_get_size(int* width, int* height) {
    if (!g_is_initialized || !width || !height) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_console_handle, &csbi)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_set_cursor(int x, int y) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    COORD pos = {(SHORT)x, (SHORT)y};
    if (!SetConsoleCursorPosition(g_console_handle, pos)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_console_show_cursor(bool visible) {
    if (!g_is_initialized) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    CONSOLE_CURSOR_INFO cci;
    if (!GetConsoleCursorInfo(g_console_handle, &cci)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    cci.bVisible = visible;
    if (!SetConsoleCursorInfo(g_console_handle, &cci)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

bool platform_console_supports_ansi(void) {
    if (!g_is_initialized) {
        return false;
    }

    DWORD mode;
    if (!GetConsoleMode(g_console_handle, &mode)) {
        return false;
    }

    return (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}