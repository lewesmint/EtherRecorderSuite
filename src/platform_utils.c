#include "platform_utils.h"
#include "platform_mutex.h"

#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    // #include <shlwapi.h>
    //// it is necessary to link with shlwapi.lib,
    //// but it is done in the project file. So pragma not required
    //// #pragma comment(lib, "shlwapi.lib")
#else // !_WIN32
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <libgen.h>
    #include <limits.h>
    #include <strings.h>
#endif // _WIN32

#ifdef _WIN32
    #define platform_mkdir(path) _mkdir(path)
    const char PATH_SEPARATOR = '\\';
#else // !_WIN32
    #define platform_mkdir(path) mkdir(path, 0755)
    const char PATH_SEPARATOR = '/'
#endif // _WIN32

// extern CRITICAL_SECTION rand_mutex;

void get_high_resolution_timestamp(LARGE_INTEGER* timestamp) {
    QueryPerformanceCounter(timestamp);
}

uint64_t platform_strtoull(const char* str, char** endptr, int base) {
    return strtoull(str, endptr, base);
}

void get_current_time(char* buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    if (t != NULL) {
        strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", t);
    }
    else {
        snprintf(buffer, buffer_size, "1970-01-01 00:00:00");
    }
}

void init_mutex(PlatformMutex_T* mutex) {
    platform_mutex_init(mutex);
}

void lock_mutex(PlatformMutex_T* mutex) {
    platform_mutex_lock(mutex);
}

void unlock_mutex(PlatformMutex_T* mutex) {
    platform_mutex_unlock(mutex);
}


char* get_cwd(char* buffer, int max_length) {
#ifdef _WIN32
    return _getcwd(buffer, max_length);
#else
    return getcwd(buffer, max_length);
#endif
}


void stream_print(FILE* stream, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (n > 0) {
        fwrite(buffer, 1, (size_t)n, stream);
    }
}

void sleep_ms(unsigned int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

void sleep_seconds(double seconds) {
    unsigned int milliseconds = (unsigned int)(seconds * 1000);
    sleep_ms(milliseconds);
}

void sanitise_path(char* path) {
    if (!path) return;

    size_t len = strlen(path);
    while (len > 0 && isspace((unsigned char)path[len - 1])) {
        path[len - 1] = '\0';
        --len;
    }

    char* start = path;
    while (*start && isspace((unsigned char)*start)) {
        ++start;
    }
    if (start != path) {
        memmove(path, start, strlen(start) + 1);
        len = strlen(path);
    }

    while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        path[len - 1] = '\0';
        --len;
    }

    for (char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = PATH_SEPARATOR;
        }
    }
}

void strip_directory_path(const char* full_file_path, char* directory_path, size_t size) {
    if (!full_file_path || !directory_path || size == 0) return;

    const char* last_sep = strrchr(full_file_path, PATH_SEPARATOR);
    if (last_sep != NULL) {
        size_t len = (size_t)(last_sep - full_file_path);
        if (len >= size) len = size - 1;
        strncpy(directory_path, full_file_path, len);
        directory_path[len] = '\0';
    }
    else {
        directory_path[0] = '\0';
    }
}

int create_directories(const char* path) {
    if (!path || !*path) return 0;

    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);

    size_t len = strlen(tmp);
    if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) {
        tmp[len - 1] = '\0';
    }

    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (platform_mkdir(tmp) != 0 && errno != EEXIST) return -1;
            *p = PATH_SEPARATOR;
        }
    }
    return (platform_mkdir(tmp) != 0 && errno != EEXIST) ? -1 : 0;
}

#ifdef _WIN32
void DisableQuickEditMode() {
    HANDLE hInput;
    DWORD prev_mode;
    hInput = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hInput, &prev_mode);
    SetConsoleMode(hInput, prev_mode & ENABLE_EXTENDED_FLAGS);
}
#endif //

#ifdef _WIN32
void EmableVTMode() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#endif //

void init_console(void) {
#ifdef _WIN32
    DisableQuickEditMode();
    EmableVTMode();
#endif
}

bool resolve_full_path(const char* filename, char* full_path, size_t size) {
#ifdef _WIN32
    return (_fullpath(full_path, filename, size) != NULL);
#else
    return (realpath(filename, full_path) != NULL);
#endif
}

/**
 * @brief Cross-platform implementation of strcasecmp()
 */
int str_cmp_nocase(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = tolower((unsigned char)*s1);
        char c2 = tolower((unsigned char)*s2);
        
        if (c1 != c2) {
            return c1 - c2;
        }
        
        s1++;
        s2++;
    }
    
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/**
 * @brief Generates a random number using the system's preferred RNG.
 * @return A random number.
 */
uint32_t platform_random() {
    uint32_t random_number = 0;

#ifdef _WIN32
    if (BCryptGenRandom(NULL, (PUCHAR)&random_number, sizeof(random_number), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        fprintf(stderr, "BCryptGenRandom failed!\n");
        return 0;  // Return 0 on failure
    }
#else
    #if defined(__linux__)
        if (getrandom(&random_number, sizeof(random_number), 0) == sizeof(random_number)) {
            return random_number;
        }
    #endif

    // Fallback to /dev/urandom if getrandom() is unavailable
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd != -1) {
        if (read(fd, &random_number, sizeof(random_number)) == sizeof(random_number)) {
            close(fd);
            return random_number;
        }
        close(fd);
    }

    // Last fallback: rand_r() with a thread-local seed
    static __thread unsigned int seed = 0;
    if (seed == 0) seed = time(NULL) ^ (uintptr_t)&seed;  // Thread-safe seed
    random_number = rand_r(&seed);
#endif // _WIN32

    return random_number;
}

/**
 * Generates a random number within a given range.
 * @param min Minimum value (inclusive).
 * @param max Maximum value (inclusive).
 * @return A random number in the range [min, max].
 */
uint32_t platform_random_range(uint32_t min, uint32_t max) {
    if (min > max) {
        uint32_t temp = min;
        min = max;
        max = temp;
    }
    return min + (platform_random() % (max - min + 1));
}
