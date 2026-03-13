---
name: "moai-lang-cpp"
description: "Modern C++ (C++23/C++20) Development Specialist — RAII, smart pointers, concepts, ranges, modules, CMake, concurrency, and testing."
source: "modu-ai/moai-adk"
---

# Modern C++ Development Specialist

Modern C++ (C++23/C++20) Development Specialist - RAII, smart pointers, concepts, ranges, modules, and CMake.

Auto-Triggers: `.cpp`, `.hpp`, `.h`, `CMakeLists.txt`, `vcpkg.json`, `conanfile.txt`, modern C++ discussions

## Core Capabilities

- **C++23 Features**: std::expected, std::print, std::generator, deducing this
- **C++20 Features**: Concepts, Ranges, Modules, Coroutines, std::format
- **Memory Safety**: RAII, Rule of 5, smart pointers (unique_ptr, shared_ptr, weak_ptr)
- **STL**: Containers, Algorithms, Iterators, std::span, std::string_view
- **Build Systems**: CMake 3.28+, FetchContent, presets
- **Concurrency**: std::thread, std::jthread, std::async, atomics, std::latch/barrier
- **Testing**: Google Test, Catch2
- **Package Management**: vcpkg, Conan 2.0

## Quick Patterns

### Smart Pointer Factory Pattern

```cpp
#include <memory>

class Widget {
public:
    static std::unique_ptr<Widget> create(int value) {
        return std::make_unique<Widget>(value);
    }
    explicit Widget(int value) : value_(value) {}
private:
    int value_;
};
```

### Concepts Constraint Pattern

```cpp
template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template<Numeric T>
T square(T value) { return value * value; }
```

### Ranges Pipeline Pattern

```cpp
auto result = std::views::iota(1, 100)
    | std::views::filter([](int n) { return n % 2 == 0; })
    | std::views::transform([](int n) { return n * n; })
    | std::views::take(10);
```

## C++23 New Features

### std::expected for Error Handling

```cpp
#include <expected>
#include <string_view>

enum class ParseError { InvalidFormat, OutOfRange };

std::expected<int, ParseError> parse_int(std::string_view sv) {
    try {
        return std::stoi(std::string(sv));
    } catch (const std::invalid_argument&) {
        return std::unexpected(ParseError::InvalidFormat);
    } catch (const std::out_of_range&) {
        return std::unexpected(ParseError::OutOfRange);
    }
}
```

### std::print for Type-Safe Output

```cpp
#include <print>
std::println("Hello, {}!", "world");
std::println("Hex: {:#x}, Float: {:.2f}", 255, 3.14159);
```

### Deducing This (Explicit Object Parameter)

```cpp
class Builder {
    std::string data_;
public:
    template<typename Self>
    auto&& append(this Self&& self, std::string_view sv) {
        self.data_ += sv;
        return std::forward<Self>(self);
    }
};
```

## RAII and Resource Management

### Rule of Five

```cpp
class Resource {
    int* data_;
    size_t size_;
public:
    Resource(size_t n) : data_(new int[n]), size_(n) {}
    ~Resource() { delete[] data_; }

    // Copy
    Resource(const Resource& other) : data_(new int[other.size_]), size_(other.size_) {
        std::copy(other.data_, other.data_ + size_, data_);
    }
    Resource& operator=(Resource other) { swap(other); return *this; }

    // Move
    Resource(Resource&& other) noexcept
        : data_(std::exchange(other.data_, nullptr)), size_(std::exchange(other.size_, 0)) {}
    Resource& operator=(Resource&& other) noexcept {
        delete[] data_;
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
        return *this;
    }

    void swap(Resource& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }
};
```

### Smart Pointer Patterns

- **unique_ptr**: Use static factory methods returning `std::unique_ptr` via `std::make_unique`
- **shared_ptr with cycles**: Use `std::enable_shared_from_this` as base class, store children in `std::vector<shared_ptr>` and parent as `std::weak_ptr` to break reference cycles

## CMake Modern Patterns

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler-specific warning flags
add_compile_options(
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# FetchContent for dependencies
include(FetchContent)
FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt GIT_TAG 10.2.1)
FetchContent_MakeAvailable(fmt)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)
```

## Concurrency

### std::jthread and Stop Tokens

```cpp
void worker(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        // do work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::jthread t(worker);
t.request_stop();  // signals termination
// destructor auto-joins
```

### Synchronization Primitives

- **std::latch**: One-time synchronization via `count_down()`
- **std::barrier**: Repeated synchronization via `arrive_and_wait()`
- **std::counting_semaphore**: Resource pools via `acquire()` / `release()`

## Context7 Library Mappings

- /microsoft/vcpkg - Package manager
- /google/googletest - Google Test framework
- /catchorg/Catch2 - Catch2 testing framework
- /fmtlib/fmt - fmt formatting library
- /nlohmann/json - JSON for Modern C++
- /gabime/spdlog - Fast logging library

## Troubleshooting

- **Version Check**: `g++ --version` (GCC 13+), `clang++ --version` (Clang 17+), `cmake --version` (3.28+)
- **Common Flags**: `-std=c++23 -Wall -Wextra -Wpedantic -O2` for standard builds, add `-fsanitize=address,undefined -g` for debugging
- **vcpkg Integration**: Configure CMake with `-DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake`
