cmake_minimum_required(VERSION 3.10)
project(ClaudeEther C)

# Set C standard based on compiler
if(MSVC)
    set(CMAKE_C_STANDARD 17)
else()
    set(CMAKE_C_STANDARD 23)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Generate compile_commands.json for better IntelliSense support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add platform_layer as a subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../platform_layer 
                 ${CMAKE_CURRENT_BINARY_DIR}/platform_layer)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform_layer/inc  # Platform layer public headers
)

# Glob source files
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Define the executable
add_executable(ClaudeEther ${SOURCES})

# Link against platform_layer and specify it as PUBLIC
target_link_libraries(ClaudeEther 
    PUBLIC platform_layer
)

# Add extra compiler warnings
if(MSVC)
    target_compile_options(ClaudeEther PRIVATE /W4)
else()
    target_compile_options(ClaudeEther PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set compile definitions based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
    message(STATUS "Configuring Debug build")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
    message(STATUS "Configuring Release build")
endif()
