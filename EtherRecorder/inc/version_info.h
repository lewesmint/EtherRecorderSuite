#ifndef VERSION_INFO_H
#define VERSION_INFO_H

// Version numbers
#define APP_VERSION_MAJOR    1
#define APP_VERSION_MINOR    0
#define APP_VERSION_PATCH    0

// Build type is automatically set by compiler flags
#ifdef NDEBUG
    #define BUILD_TYPE "Release"
#else
    #define BUILD_TYPE "Debug"
#endif

// These macros are automatically populated with build date and time
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// Helper function to print version info
void print_version_info(void);

#endif // VERSION_INFO_H
