---
name: code-quality
description: "Code quality detection and improvement — static analysis, formatting, linting, best practices, and automated checks for clean, maintainable code."
source: "miles990/claude-software-skills"
---

# Code Quality

Comprehensive code quality skill covering static analysis, formatting, linting, code review, and best practices enforcement.

Auto-Triggers: Code review requests, quality checks, formatting issues, lint errors, best practices discussions

## Core Principles

1. **Readability** — Code is read far more than written. Optimize for the reader.
2. **Consistency** — Follow established patterns in the codebase. Don't mix styles.
3. **Simplicity** — Prefer simple, obvious solutions over clever ones.
4. **Testability** — Write code that is easy to test in isolation.
5. **DRY** — Don't Repeat Yourself, but don't over-abstract either.

## Code Review Checklist

### Correctness
- [ ] Logic is correct and handles edge cases
- [ ] Error handling is comprehensive (no swallowed errors)
- [ ] Resource cleanup (RAII, `finally`, `defer`, destructors)
- [ ] Thread safety (if applicable)
- [ ] No undefined behavior (C/C++)

### Maintainability
- [ ] Functions/methods are small and focused (single responsibility)
- [ ] Variable/function names are descriptive and consistent
- [ ] No magic numbers — use named constants
- [ ] Complex logic has explanatory comments (why, not what)
- [ ] No dead code or commented-out code

### Performance
- [ ] No obvious O(n²) where O(n) is possible
- [ ] No unnecessary copies of large objects
- [ ] No repeated expensive operations that could be cached
- [ ] Database queries are efficient (no N+1 queries)

### Security
- [ ] Input is validated and sanitized
- [ ] No hardcoded secrets or credentials
- [ ] SQL queries use parameterized statements
- [ ] File paths are validated (no path traversal)

## Static Analysis Tools

### C/C++

```bash
# Clang-Tidy — comprehensive C++ linter
clang-tidy src/*.cpp -- -std=c++17 -I include/

# Cppcheck — static analysis
cppcheck --enable=all --std=c++17 src/

# Include What You Use
iwyu_tool.py -p build/
```

`.clang-tidy` configuration:
```yaml
Checks: >
  -*,
  bugprone-*,
  cert-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type

WarningsAsErrors: ''
HeaderFilterRegex: 'src/.*'
```

### Python

```bash
# Ruff — fast Python linter + formatter
ruff check src/
ruff format src/

# MyPy — type checking
mypy src/ --strict

# Pylint
pylint src/
```

### JavaScript / TypeScript

```bash
# ESLint
npx eslint src/ --fix

# Prettier — code formatting
npx prettier --write "src/**/*.{ts,tsx,js,jsx}"

# TypeScript strict checks
tsc --noEmit --strict
```

### General

```bash
# EditorConfig — cross-editor formatting
# Place .editorconfig in project root

# Pre-commit hooks
pre-commit install
pre-commit run --all-files
```

## Formatting Standards

### .editorconfig

```ini
root = true

[*]
indent_style = space
indent_size = 4
end_of_line = lf
charset = utf-8
trim_trailing_whitespace = true
insert_final_newline = true

[*.{js,ts,jsx,tsx,json,yml,yaml}]
indent_size = 2

[*.md]
trim_trailing_whitespace = false

[Makefile]
indent_style = tab
```

### C++ Formatting (.clang-format)

```yaml
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 120
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: Never
BreakBeforeBraces: Attach
PointerAlignment: Left
SortIncludes: CaseInsensitive
IncludeBlocks: Regroup
```

## Common Code Smells

### Overly Long Functions
- **Smell**: Function > 50 lines
- **Fix**: Extract helper functions with descriptive names

### Deep Nesting
- **Smell**: More than 3 levels of indentation
- **Fix**: Early returns (guard clauses), extract methods

```cpp
// BAD — deep nesting
void process(Data* data) {
    if (data) {
        if (data->isValid()) {
            if (data->hasItems()) {
                for (auto& item : data->items()) {
                    // ...
                }
            }
        }
    }
}

// GOOD — guard clauses
void process(Data* data) {
    if (!data) return;
    if (!data->isValid()) return;
    if (!data->hasItems()) return;

    for (auto& item : data->items()) {
        // ...
    }
}
```

### God Class
- **Smell**: Class with too many responsibilities (> 500 lines)
- **Fix**: Split into focused classes, apply Single Responsibility Principle

### Primitive Obsession
- **Smell**: Using raw strings/ints for domain concepts
- **Fix**: Create domain types (e.g., `Email`, `UserId`, `Temperature`)

### Feature Envy
- **Smell**: Method uses more data from another class than its own
- **Fix**: Move method to the class whose data it uses

## Naming Conventions

| Element | C++ | Python | TypeScript |
|---------|-----|--------|------------|
| Class | `PascalCase` | `PascalCase` | `PascalCase` |
| Function | `camelCase` or `snake_case` | `snake_case` | `camelCase` |
| Variable | `camelCase` or `snake_case` | `snake_case` | `camelCase` |
| Constant | `UPPER_SNAKE` | `UPPER_SNAKE` | `UPPER_SNAKE` |
| Private member | `m_name` or `name_` | `_name` | `_name` or `#name` |
| File | `snake_case.cpp` | `snake_case.py` | `kebab-case.ts` |

## Metrics and Thresholds

| Metric | Healthy | Warning | Critical |
|--------|---------|---------|----------|
| Cyclomatic complexity | < 10 | 10-20 | > 20 |
| Function length | < 50 lines | 50-100 | > 100 |
| File length | < 500 lines | 500-1000 | > 1000 |
| Nesting depth | ≤ 3 | 4 | ≥ 5 |
| Parameters per function | ≤ 4 | 5-6 | ≥ 7 |
| Test coverage | > 80% | 60-80% | < 60% |

## Git Hygiene

- **Atomic commits** — Each commit does one thing
- **Descriptive messages** — Use conventional commits format
- **No large files** — Use `.gitignore` and Git LFS for binaries
- **No secrets** — Use `.env` files excluded from version control
- **Clean history** — Squash fixup commits before merging
