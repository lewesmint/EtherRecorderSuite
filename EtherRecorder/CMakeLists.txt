cmake_minimum_required(VERSION 3.10)
project(EtherRecorder C)

# Force x64 architecture
set(CMAKE_SYSTEM_PROCESSOR "AMD64")
message(STATUS "EtherRecorder - Building for x64 architecture")

# Set C standard consistently for all platforms
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Set compiler flags based on build type and platform
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        set(CMAKE_C_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1 /D_DEBUG")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
        add_compile_options(/diagnostics:column)
    else()
        set(CMAKE_C_FLAGS_DEBUG "-g -O0 -D_DEBUG")
    endif()
else()
    if(MSVC)
        set(CMAKE_C_FLAGS_RELEASE "/MT /O2 /Ob2 /DNDEBUG")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
        add_compile_options(/diagnostics:column)
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

# Add platform-specific defines
if(APPLE)
    add_compile_definitions(
        MACOS=1
        POSIX=1
        _DARWIN_C_SOURCE
    )
    # Force x86_64 architecture on macOS
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures for macOS" FORCE)
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

# Add PlatformLayer as a subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../PlatformLayer 
                 ${CMAKE_CURRENT_BINARY_DIR}/PlatformLayer)

# Define the executable first
add_executable(EtherRecorder)

# Add source files
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
target_sources(EtherRecorder PRIVATE ${SOURCES})

# Set include directories
target_include_directories(EtherRecorder
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../PlatformLayer/inc
)

# Link against PlatformLayer
target_link_libraries(EtherRecorder
    PUBLIC PlatformLayer
)

# Add extra compiler warnings
if(MSVC)
    target_compile_options(EtherRecorder PRIVATE /W4)
else()
    target_compile_options(EtherRecorder PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set compile definitions based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
    message(STATUS "Configuring Debug build")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
    message(STATUS "Configuring Release build")
endif()

# Output architecture information for debugging
message(STATUS "Target architecture: ${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "Building for 64-bit architecture")
else()
    message(STATUS "Building for 32-bit architecture")
endif()
