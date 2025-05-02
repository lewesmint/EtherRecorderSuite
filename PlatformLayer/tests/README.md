# Windows Shared Memory Test

This directory contains scripts for testing the Windows shared memory functionality.

## Running the Tests

### Option 1: Automated Test (Recommended)

The simplest way to run the test is to use the automated batch file:

```
run_shared_memory_test.bat
```

This will:
1. Start the C test program to create the shared memory
2. Run the Python monitoring script to connect to the shared memory

### Option 2: Manual Testing

If you prefer to run the tests manually:

1. First, run the C test program to create the shared memory:
   ```
   ..\build\Debug\bin\test_win_shared_memory.exe
   ```

2. Then, in a separate terminal window, run the Python monitoring script:
   ```
   monitor_shared_memory.bat
   ```

   Or directly:
   ```
   python test_win_shared_memory.py
   ```

   The script will use the settings from `shared_memory_config.jsonc`

## Additional Options

You can run the Python script with various options:

```
# Show all available options
python test_win_shared_memory.py --help
```

## Troubleshooting

If you encounter issues:

1. Make sure the C test program is built correctly:
   ```
   cd ..\build\Debug
   cmake --build . --target test_win_shared_memory --config Debug
   ```

2. Check that the C test program is creating the shared memory with the expected name ("TestSharedMemory")

3. Use the `--wait` flag with the Python script to keep retrying until the shared memory is available
