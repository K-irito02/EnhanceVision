# Contributing Guide

English | [简体中文](CONTRIBUTING_CN.md)

Thank you for your interest in contributing to EnhanceVision! This document will help you understand how to participate in the project development.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How to Contribute](#how-to-contribute)
- [Development Environment Setup](#development-environment-setup)
- [Code Standards](#code-standards)
- [Commit Guidelines](#commit-guidelines)
- [Pull Request Process](#pull-request-process)

## Code of Conduct

This project adopts the Contributor Covenant as its code of conduct. By participating in this project, you agree to abide by its terms. Please read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for details.

## How to Contribute

### Reporting Bugs

If you find a bug, please submit a report via [GitHub Issues](https://github.com/K-irito02/EnhanceVision/issues). Before submitting:

1. Search existing issues to confirm the problem hasn't been reported
2. Use the issue template to provide detailed information:
   - Operating system and version
   - EnhanceVision version
   - Steps to reproduce
   - Expected behavior vs actual behavior
   - Screenshots or logs (if applicable)

### Suggesting New Features

New feature suggestions are welcome! Please submit via GitHub Issues with:

1. Feature description
2. Use case
3. Possible implementation approach (optional)

### Submitting Code

1. Fork this repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'feat: add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Create a Pull Request

## Development Environment Setup

### System Requirements

- Windows 10/11 64-bit
- Visual Studio 2022 (MSVC)
- CMake 3.20+
- Qt 6.10.2
- Vulkan SDK

### Build Steps

```powershell
# Clone repository
git clone https://github.com/K-irito02/EnhanceVision.git
cd EnhanceVision

# Initialize submodules
git submodule update --init --recursive

# Debug build
cmake --preset windows-msvc-2022-debug
cmake --build build/msvc2022/Debug --config Debug -j 8

# Release build
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

### Running Tests

```powershell
ctest --test-dir build/msvc2022/Debug -C Debug --output-on-failure
```

## Code Standards

### C++ Standards

See [.trae/rules/03-cpp-standards.md](.trae/rules/03-cpp-standards.md)

Key points:
- Use C++20 features
- Follow RAII principles
- Use smart pointers for memory management
- Add appropriate comments

### QML Standards

See [.trae/rules/04-qml-standards.md](.trae/rules/04-qml-standards.md)

Key points:
- Use declarative syntax
- Single responsibility for components
- Use camelCase for property names

### Code Formatting

Ensure code is properly formatted before submitting:

```powershell
# Format C++ code with clang-format
clang-format -i src/**/*.cpp include/**/*.h
```

## Commit Guidelines

This project follows [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

### Types

| Type | Description |
|------|-------------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation update |
| `style` | Code formatting (no functionality change) |
| `refactor` | Code refactoring |
| `perf` | Performance optimization |
| `test` | Test-related |
| `chore` | Build/tool-related |

### Example

```
feat(ai): add Real-ESRGAN model support

- Add model loading for Real-ESRGAN x4plus
- Implement image preprocessing pipeline
- Support GPU acceleration via Vulkan

Closes #123
```

## Pull Request Process

1. **Ensure all checks pass**
   - Code compiles without errors
   - All tests pass
   - Code formatting follows standards

2. **PR title format**
   - Use Conventional Commits format
   - Example: `feat: add batch export feature`

3. **PR description template**
   ```markdown
   ## Change Type
   - [ ] Bug fix
   - [ ] New feature
   - [ ] Refactoring
   - [ ] Documentation update

   ## Description
   [Describe your changes]

   ## Testing
   [Describe how to test these changes]

   ## Screenshots (if applicable)
   [Add screenshots]

   ## Related Issue
   Closes #
   ```

4. **Code Review**
   - At least one maintainer review required
   - Address all review comments
   - Keep commit history clean (use squash if necessary)

## License

By contributing code, you agree that your contributions will be licensed under the MIT License.
