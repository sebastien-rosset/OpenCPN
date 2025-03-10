# GRIB Plugin Unit Tests

This directory contains unit tests for the OpenCPN GRIB plugin. The tests use Google Test framework and are integrated with the plugin's build system.

## Building and Running Tests

### Unix-like Systems (Linux, macOS)

1. Configure and build the plugin as usual:

```bash
cd build
cmake ..
make
```

2. Run the tests using either:

```bash
cd plugins/grib_pi

# Run via ctest
ctest

# Or run the test executable directly
./tests/grib_pi_tests
```

### Windows

On Windows, some additional setup may be required:

1. Ensure the PATH environment variable includes:
   - CMake binary directory (usually `C:\Program Files\CMake\bin`)
   - Required DLL directories (OpenCPN buildwin directory)

2. Build and run tests:

```cmd
cd build
cmake --build . --config RelWithDebInfo
cmake --build . --target=run-tests --config RelWithDebInfo
```

## Adding New Tests

1. Create a new test file in the `tests` directory:

```cpp
#include <gtest/gtest.h>
#include "your_header.h"

TEST(YourTestSuite, TestName) {
    // Your test code here
    EXPECT_TRUE(true);
}
```

2. Add the new test file to `tests/CMakeLists.txt`:

```cmake
add_executable(grib_pi_tests
    grib_layer_test.cpp
    grib_layer_set_test.cpp
    your_new_test.cpp  # Add your test file here
)
```
