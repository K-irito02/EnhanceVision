# Version Management Guidelines

English | [简体中文](06-version-management_CN.md)

This document describes the version management guidelines for EnhanceVision, following Semantic Versioning.

## 📋 Table of Contents

- [Version Number Format](#version-number-format)
- [Version Increment Rules](#version-increment-rules)
- [Release Cycle](#release-cycle)
- [Version Branching Strategy](#version-branching-strategy)
- [Changelog Management](#changelog-management)

---

## Version Number Format

### Standard Format

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
```

### Components

| Part | Description | Example |
|------|-------------|---------|
| MAJOR | Major version number | 0, 1, 2 |
| MINOR | Minor version number | 1, 2, 3 |
| PATCH | Patch version number | 0, 1, 2 |
| PRERELEASE | Pre-release identifier (optional) | alpha, beta, rc |
| BUILD | Build metadata (optional) | build.123 |

### Examples

| Version | Description |
|---------|-------------|
| `0.1.0` | First release |
| `0.1.1` | Bug fix release |
| `0.2.0` | New feature release |
| `1.0.0` | Stable release |
| `1.0.0-alpha` | Internal testing version |
| `1.0.0-beta.1` | Public testing version |
| `1.0.0-rc.1` | Release candidate |

---

## Version Increment Rules

### MAJOR (Major Version)

**Increment Conditions:**
- Incompatible API changes
- Major architecture changes
- Removal of important features

**Examples:**
- 0.x.x → 1.0.0: First stable release
- 1.x.x → 2.0.0: Architecture refactor, incompatible with previous version

### MINOR (Minor Version)

**Increment Conditions:**
- Backward-compatible new features
- Important feature improvements
- Performance optimizations

**Examples:**
- 0.1.0 → 0.2.0: Add new AI model support
- 1.0.0 → 1.1.0: Add batch export feature

### PATCH (Patch Version)

**Increment Conditions:**
- Backward-compatible bug fixes
- Minor improvements
- Documentation updates

**Examples:**
- 0.1.0 → 0.1.1: Fix video export crash
- 1.0.0 → 1.0.1: Fix memory leak

---

## Release Cycle

### Version Stages

```
Development → Testing → Release
    │            │           │
    ▼            ▼           ▼
  alpha       beta/rc     stable
```

### Pre-release Versions

| Identifier | Description | Target Users |
|------------|-------------|--------------|
| `alpha` | Internal testing, incomplete features | Development team |
| `beta` | Public testing, mostly complete features | Test users |
| `rc` | Release candidate, expected final version | Early adopters |

### Version Lifecycle

| Version Type | Support Period | Update Types |
|--------------|----------------|--------------|
| Major (MAJOR) | 2 years | Security updates, bug fixes |
| Minor (MINOR) | 1 year | Bug fixes |
| Patch (PATCH) | 6 months | Emergency fixes |

---

## Version Branching Strategy

### Branch Model

```
main (stable branch)
  │
  ├── develop (development branch)
  │     │
  │     ├── feature/xxx (feature branch)
  │     ├── fix/xxx (fix branch)
  │     └── refactor/xxx (refactor branch)
  │
  └── release/x.y (release branch)
```

### Branch Descriptions

| Branch | Purpose | Merge Target |
|--------|---------|--------------|
| `main` | Stable release versions | - |
| `develop` | Code in development | `main` |
| `feature/*` | New feature development | `develop` |
| `fix/*` | Bug fixes | `develop` or `release/*` |
| `release/*` | Release preparation | `main` and `develop` |

### Release Process

```
1. Create release/x.y branch from develop
2. Test and fix in release/x.y branch
3. After testing, merge to main
4. Tag main with v.x.y.z
5. Publish to GitHub Releases
6. Merge fixes back to develop
```

---

## Changelog Management

### CHANGELOG.md Format

Follow [Keep a Changelog](https://keepachangelog.com/) specification:

```markdown
# Changelog

## [Unreleased]

### Added
- New feature description

### Changed
- Change description

### Deprecated
- Features to be deprecated

### Removed
- Removed features

### Fixed
- Fix description

### Security
- Security-related fixes

---

## [1.0.0] - 2025-01-15

### Added
- First stable release

---

## [0.2.0] - 2025-01-01

### Added
- Add batch export feature

### Fixed
- Fix video processing crash
```

### Change Type Descriptions

| Type | Description |
|------|-------------|
| `Added` | New features |
| `Changed` | Changes to existing features |
| `Deprecated` | Features to be removed soon |
| `Removed` | Removed features |
| `Fixed` | Bug fixes |
| `Security` | Security-related fixes |

---

## Version Update Process

### 1. Update CMakeLists.txt

```cmake
# Update version number
project(EnhanceVision VERSION 0.2.0 LANGUAGES CXX)
```

### 2. Update CHANGELOG.md

```markdown
## [0.2.0] - 2025-02-01

### Added
- Add batch export feature
- Add new AI model support

### Fixed
- Fix video processing crash
```

### 3. Create Git Tag

```powershell
git tag -a v0.2.0 -m "Release v0.2.0"
git push origin v0.2.0
```

### 4. Publish to GitHub

Refer to [04-github-release-guide.md](04-github-release-guide.md).

---

## Pre-release Version Management

### Create Pre-release Version

```powershell
# Create alpha version
git tag -a v1.0.0-alpha.1 -m "Alpha release for v1.0.0"
git push origin v1.0.0-alpha.1

# Create beta version
git tag -a v1.0.0-beta.1 -m "Beta release for v1.0.0"
git push origin v1.0.0-beta.1

# Create rc version
git tag -a v1.0.0-rc.1 -m "Release candidate for v1.0.0"
git push origin v1.0.0-rc.1
```

### Pre-release Naming Rules

| Stage | Format | Example |
|-------|--------|---------|
| Alpha | `vX.Y.Z-alpha.N` | v1.0.0-alpha.1 |
| Beta | `vX.Y.Z-beta.N` | v1.0.0-beta.1 |
| RC | `vX.Y.Z-rc.N` | v1.0.0-rc.1 |

---

## Version Check Script

Create `scripts\check-version.ps1`:

```powershell
param(
    [string]$ExpectedVersion
)

# Read version from CMakeLists.txt
$cmakeContent = Get-Content "CMakeLists.txt" -Raw
if ($cmakeContent -match 'project\(EnhanceVision VERSION (\d+\.\d+\.\d+)') {
    $cmakeVersion = $matches[1]
} else {
    Write-Error "Cannot read version from CMakeLists.txt"
    exit 1
}

# Read latest version from CHANGELOG.md
$changelogContent = Get-Content "CHANGELOG.md" -Raw
if ($changelogContent -match '## \[(\d+\.\d+\.\d+)\]') {
    $changelogVersion = $matches[1]
} else {
    Write-Error "Cannot read version from CHANGELOG.md"
    exit 1
}

# Check version consistency
Write-Host "CMakeLists.txt version: $cmakeVersion"
Write-Host "CHANGELOG.md version: $changelogVersion"

if ($cmakeVersion -ne $changelogVersion) {
    Write-Error "Version mismatch!"
    exit 1
}

if ($ExpectedVersion -and $cmakeVersion -ne $ExpectedVersion) {
    Write-Error "Version doesn't match expected! Expected: $ExpectedVersion, Actual: $cmakeVersion"
    exit 1
}

Write-Host "✅ Version check passed: $cmakeVersion" -ForegroundColor Green
```

Run check:
```powershell
.\scripts\check-version.ps1 -ExpectedVersion "0.1.0"
```

---

## Version Planning Example

### 0.x.x Series (Development Phase)

| Version | Planned Content | Expected Date |
|---------|-----------------|---------------|
| 0.1.0 | First release | 2025-01 |
| 0.1.1 | Bug fixes | 2025-02 |
| 0.2.0 | New AI model | 2025-03 |
| 0.3.0 | Video processing optimization | 2025-04 |

### 1.x.x Series (Stable Phase)

| Version | Planned Content | Expected Date |
|---------|-----------------|---------------|
| 1.0.0 | Stable version | 2025-06 |
| 1.1.0 | Enhanced batch processing | 2025-07 |
| 1.2.0 | Plugin system | 2025-09 |

---

## FAQ

### Q: When to upgrade from 0.x.x to 1.0.0?

A: When the software meets these conditions:
- API is stable
- Core features are complete
- Documentation is comprehensive
- Thoroughly tested

### Q: How to handle emergency fixes?

A: Create a hotfix branch from the latest stable version:
```powershell
git checkout -b hotfix/1.0.1 v1.0.0
# Fix the issue
git commit -m "fix: critical bug"
git tag -a v1.0.1 -m "Hotfix release"
git push origin v1.0.1
```

### Q: Do pre-release versions need CHANGELOG?

A: Yes. Pre-release changes are recorded in the `[Unreleased]` section.

---

## References

- [Semantic Versioning](https://semver.org/)
- [Keep a Changelog](https://keepachangelog.com/)
- [Git Flow](https://nvie.com/posts/a-successful-git-branching-model/)

---

*Last updated: 2025*
