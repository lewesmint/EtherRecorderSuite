cmake_minimum_required(VERSION 3.10)
project(ClaudeEther C)

# Set C23 standard
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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
