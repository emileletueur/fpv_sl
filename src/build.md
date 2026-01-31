### Verify
cmake --version
ninja --version
arm-none-eabi-gcc --version
echo %PICO_SDK_PATH%

### Cleanup
rm -rf build
Remove-Item -Recurse -Force build //windows

### Init configuration
cmake -S . -B build -G Ninja

### Build project
cmake --build build
