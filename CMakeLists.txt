cmake_minimum_required(VERSION 3.10)
project(ClaudeEther C)


# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Windows-specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0600 -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
    set(WINDOWS_LIBS ws2_32 winmm)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

# Gather all source files
file(GLOB SOURCES "src/*.c")

# Define the executable
add_executable(ClaudeEther ${SOURCES})

# Add extra compiler warnings
if(MSVC)
    target_compile_options(ClaudeEther PRIVATE /W4)
else()
    target_compile_options(ClaudeEther PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Link Windows libraries if on Windows
if(WIN32)
    target_link_libraries(ClaudeEther PRIVATE bcrypt ws2_32)
endif()