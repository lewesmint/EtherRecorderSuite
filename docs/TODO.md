# TODO

## Build System Improvements

- Test platform-independent tasks.json configuration using `${command:cmake.buildKit}`
  - Should automatically handle both Windows ARM64 and MacOS ARM64 builds
  - Located in `.vscode/tasks.json`
  - Will eliminate need to manually switch build configurations between platforms
  - Current implementation is in chat history dated [current_date]