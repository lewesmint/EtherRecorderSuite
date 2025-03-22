cmake_minimum_required(VERSION 3.10)
project(PlatformLayer C)

# Set C standard consistently for all platforms
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set compiler flags based on build type and platform
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        set(CMAKE_C_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1 /D_DEBUG")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
    else()
        set(CMAKE_C_FLAGS_DEBUG "-g -O0 -D_DEBUG")
    endif()
else()
    if(MSVC)
        set(CMAKE_C_FLAGS_RELEASE "/MT /O2 /Ob2 /DNDEBUG")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    else()
        set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    endif()
endif()

# Remove /MD from compiler options if it exists
if(MSVC)
    foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
endif()

# Define header directories
set(PLATFORM_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/posix/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/macos/inc
)

# Add platform-specific defines
if(APPLE)
    add_compile_definitions(
        MACOS=1
        POSIX=1
        _DARWIN_C_SOURCE
    )
elseif(WIN32)
    add_compile_definitions(
        WIN32=1
        _WIN32_WINNT=0x0600
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
else()
    add_compile_definitions(
        POSIX=1
    )
endif()

# Common sources
file(GLOB_RECURSE COMMON_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Platform-specific sources
if(WIN32)
    file(GLOB_RECURSE PLATFORM_SOURCES 
        "windows/src/*.c"
        "windows/src/**/*.c"
    )
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SOURCES 
        "posix/src/*.c"
        "posix/src/**/*.c"
        "macos/src/*.c"
        "macos/src/**/*.c"
    )
else()
    file(GLOB_RECURSE PLATFORM_SOURCES 
        "posix/src/*.c"
        "posix/src/**/*.c"
    )
endif()

# Create the library with both common and platform-specific sources
add_library(PlatformLayer ${COMMON_SOURCES} ${PLATFORM_SOURCES})

# Set include directories using target_include_directories
target_include_directories(PlatformLayer
    PUBLIC
        ${PLATFORM_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add source groups for better organization in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PLATFORM_SOURCES})

# Add header files to the project for better IDE integration
file(GLOB_RECURSE HEADER_FILES
    "inc/*.h"
    "windows/inc/*.h"
    "posix/inc/*.h"
    "macos/inc/*.h"
)
target_sources(PlatformLayer PRIVATE ${HEADER_FILES})

# Create source groups for headers
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADER_FILES})

# Add compiler warnings
if(MSVC)
    target_compile_options(PlatformLayer PRIVATE 
        /W4 
        /WX      # Treat warnings as errors
        /wd4100  # Unreferenced formal parameter
        /wd4200  # Zero-sized array in struct/union
        /guard:cf # Control Flow Guard
        /sdl     # Additional security checks
        /MP      # Multi-processor compilation
    )
else()
    target_compile_options(PlatformLayer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(PlatformLayer PRIVATE 
        shlwapi     # Shell API
        bcrypt      # Cryptography
        ws2_32      # Windows Sockets
        synchronization  # WaitOnAddress and related functions
        userenv     # User environment functions
    )

    # Enable Address Space Layout Randomization (ASLR)
    if(MSVC)
        set_target_properties(PlatformLayer PROPERTIES LINK_FLAGS "/DYNAMICBASE")
    endif()
endif()

# Install rules
install(TARGETS PlatformLayer
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY inc/
    DESTINATION include/PlatformLayer
    FILES_MATCHING PATTERN "*.h"
)

# if(MSVC)
#     add_custom_command(
#         TARGET PlatformLayer POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different
#             "${CMAKE_BINARY_DIR}/compile_commands.json"
#             "${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json"
#         COMMENT "Copying compile_commands.json to source directory"
#     )
# endif()
