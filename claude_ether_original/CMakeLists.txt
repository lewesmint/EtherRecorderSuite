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

# Add include directories
target_include_directories(ClaudeEther PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# Link Windows libraries if on Windows
if(WIN32)
    target_link_libraries(ClaudeEther PRIVATE shlwapi bcrypt ws2_32)
endif() 
