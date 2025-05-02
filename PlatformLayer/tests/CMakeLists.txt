# Add test_win_shared_memory executable
add_executable(test_win_shared_memory
    test_win_shared_memory.c
)

# Include directories
target_include_directories(test_win_shared_memory PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
)

# Link with PlatformLayer library
target_link_libraries(test_win_shared_memory PRIVATE
    PlatformLayer
)

# Set output directory for all configurations to avoid nested Debug/Release folders
set_target_properties(test_win_shared_memory PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)
