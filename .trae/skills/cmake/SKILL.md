---
name: cmake
description: "CMake build system expert — project configuration, targets, dependencies, presets, toolchains, and cross-compilation for C/C++ projects."
source: "mohitmishra786/low-level-dev-skills"
---

# CMake Build System

Expert guidance for CMake-based C/C++ project configuration, building, and dependency management.

Auto-Triggers: `CMakeLists.txt`, `*.cmake`, `CMakePresets.json`, `cmake` commands, build system discussions

## Core Concepts

### Minimum Modern CMake

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyProject
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "My C++ Project"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

### Targets and Properties

Modern CMake is target-based. Avoid global commands like `include_directories()` and `link_libraries()`.

```cmake
add_executable(myapp src/main.cpp src/app.cpp)
add_library(mylib STATIC src/lib.cpp)

# Per-target properties (PREFERRED)
target_include_directories(mylib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(mylib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(mylib PUBLIC cxx_std_17)

# Link libraries — propagates include dirs, compile defs, etc.
target_link_libraries(myapp PRIVATE mylib)
```

### Visibility Keywords

| Keyword | Meaning |
|---------|---------|
| `PUBLIC` | Used by this target AND consumers |
| `PRIVATE` | Used only by this target |
| `INTERFACE` | Used only by consumers, not this target |

### find_package

```cmake
# Config mode (preferred, looks for <Package>Config.cmake)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Module mode (fallback, uses Find<Package>.cmake)
find_package(OpenCV REQUIRED)

# Optional package
find_package(Doxygen QUIET)
if(Doxygen_FOUND)
    # ...
endif()
```

## FetchContent (Download Dependencies at Configure Time)

```cmake
include(FetchContent)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(spdlog googletest)

target_link_libraries(myapp PRIVATE spdlog::spdlog)
target_link_libraries(mytest PRIVATE GTest::gtest_main)
```

## CMake Presets

`CMakePresets.json` standardizes configuration across machines:

```json
{
    "version": 6,
    "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
    "configurePresets": [
        {
            "name": "default",
            "displayName": "Default Config",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/${presetName}",
            "cacheVariables": {
                "CMAKE_CXX_STANDARD": "17",
                "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
            }
        },
        {
            "name": "debug",
            "inherits": "default",
            "displayName": "Debug",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug"
            }
        },
        {
            "name": "release",
            "inherits": "default",
            "displayName": "Release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release"
            }
        },
        {
            "name": "msvc",
            "displayName": "MSVC (Visual Studio)",
            "generator": "Visual Studio 17 2022",
            "binaryDir": "${sourceDir}/build/${presetName}",
            "architecture": { "value": "x64", "strategy": "set" },
            "cacheVariables": {
                "CMAKE_CXX_STANDARD": "17"
            }
        }
    ],
    "buildPresets": [
        { "name": "debug-build",   "configurePreset": "debug" },
        { "name": "release-build", "configurePreset": "release" }
    ]
}
```

Usage:
```bash
cmake --preset debug
cmake --build --preset debug-build
```

## Custom Commands and Targets

```cmake
# Generate a file at build time
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/generated.h
    COMMAND ${CMAKE_COMMAND} -E echo "#define VERSION \"1.0\"" > generated.h
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating version header"
)

# Custom target (always runs)
add_custom_target(format
    COMMAND clang-format -i ${ALL_SOURCE_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format"
)
```

## Installation

```cmake
include(GNUInstallDirs)

install(TARGETS mylib myapp
    EXPORT MyProjectTargets
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
```

## Compiler Warnings

```cmake
# Per-target warnings (preferred)
target_compile_options(mylib PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
```

## Testing with CTest

```cmake
enable_testing()

add_executable(mytest tests/test_main.cpp)
target_link_libraries(mytest PRIVATE mylib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mytest)
```

```bash
cmake --build build
cd build && ctest --output-on-failure
```

## Common Mistakes

| Mistake | Fix |
|---------|-----|
| `include_directories()` globally | Use `target_include_directories()` per target |
| `link_libraries()` globally | Use `target_link_libraries()` per target |
| Missing `CMAKE_CXX_STANDARD_REQUIRED` | Always set to `ON` |
| `file(GLOB ...)` for sources | List source files explicitly |
| Mixing `add_subdirectory` with `FetchContent` for same dep | Pick one approach |
| Not using `GNUInstallDirs` | Always include for portable install paths |

## Quick Reference

```bash
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release -j$(nproc)

# Install
cmake --install build --prefix /usr/local

# Clean
cmake --build build --target clean

# List available targets
cmake --build build --target help

# Verbose build
cmake --build build --verbose
```
