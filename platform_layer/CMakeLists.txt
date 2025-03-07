cmake_minimum_required(VERSION 3.10)
project(platform_layer C)

# Set C23 standard
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows-specific settings
if(WIN32)
    add_definitions(
        -D_WIN32_WINNT=0x0600 
        -DNOMINMAX 
        -DWIN32_LEAN_AND_MEAN 
        -D_CRT_SECURE_NO_WARNINGS
    )
endif()

# Include directories - make sure both public and private headers are found
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/posix/inc  # Add private POSIX headers
)

# Platform-specific sources
if(WIN32)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/windows/inc)
    file(GLOB PLATFORM_SOURCES "windows/src/*.c")
elseif(APPLE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/macos/inc)
    file(GLOB PLATFORM_SOURCES 
        "posix/src/*.c"
        "macos/src/*.c"
    )
else()
    file(GLOB PLATFORM_SOURCES "posix/src/*.c")
endif()

# Create the library
add_library(platform_layer ${PLATFORM_SOURCES})

# Add compiler warnings
if(MSVC)
    target_compile_options(platform_layer PRIVATE /W4)
else()
    target_compile_options(platform_layer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(platform_layer PRIVATE shlwapi bcrypt ws2_32)
endif()
