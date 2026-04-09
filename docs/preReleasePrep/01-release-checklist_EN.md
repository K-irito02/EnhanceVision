# Release Checklist

English | [简体中文](01-release-checklist_CN.md)

This document lists the items that must be checked before each release to ensure release quality.

## 📋 Pre-Release Checklist

### 1. Code Quality Checks

#### 1.1 Compilation Checks
- [ ] Release build compiles without errors
- [ ] Release build compiles without warnings (or known warnings are documented)
- [ ] All unit tests pass
- [ ] Static code analysis passes (if configured)

```powershell
# Run compilation check
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8

# Run tests
ctest --test-dir build/msvc2022/Release -C Release --output-on-failure
```

#### 1.2 Code Review
- [ ] All code changes have been reviewed via Pull Request
- [ ] No TODO or FIXME markers (or documented as known issues)
- [ ] Code formatting follows standards
- [ ] No hardcoded debug code or test data

#### 1.3 Security Checks
- [ ] No sensitive information leakage (API keys, passwords, etc.)
- [ ] No known security vulnerabilities
- [ ] Third-party dependency versions have no known security issues

---

### 2. Functional Testing

#### 2.1 Core Feature Testing
- [ ] Image Shader processing works correctly
  - [ ] All 14 Shader parameters adjust normally
  - [ ] Real-time preview is smooth
  - [ ] Parameter save/restore works
- [ ] Image AI processing works correctly
  - [ ] Real-ESRGAN model loads normally
  - [ ] GPU inference works
  - [ ] CPU inference works
  - [ ] Inference mode switching doesn't crash
- [ ] Video processing works correctly
  - [ ] Shader video processing works
  - [ ] AI video processing works
  - [ ] Video export works
  - [ ] Audio is preserved

#### 2.2 File Operation Testing
- [ ] Add files (drag & drop, button)
- [ ] Batch file processing
- [ ] File export (various formats)
- [ ] File deletion

#### 2.3 UI Feature Testing
- [ ] Dark/Light theme switching works
- [ ] Chinese/English switching works
- [ ] Session management works
  - [ ] Create session
  - [ ] Switch session
  - [ ] Delete session
  - [ ] Pin session
- [ ] Media viewer works
  - [ ] Embedded mode
  - [ ] Drag-out
  - [ ] Fullscreen mode
- [ ] Settings page works
  - [ ] Settings save
  - [ ] Settings restore

#### 2.4 Exception Handling Testing
- [ ] Large file processing doesn't crash
- [ ] GPU OOM auto-recovery works
- [ ] Task pause/resume works
- [ ] Task recovery after app restart works
- [ ] Warning prompt after abnormal close works

---

### 3. Performance Testing

#### 3.1 Startup Performance
- [ ] Cold start time < 3 seconds
- [ ] Warm start time < 1 second

#### 3.2 Runtime Performance
- [ ] Memory usage is reasonable (< 500MB idle)
- [ ] CPU usage is reasonable (idle < 5%)
- [ ] GPU usage is reasonable (idle < 5%)
- [ ] No memory leaks

#### 3.3 Processing Performance
- [ ] Image Shader processing is smooth (> 30 FPS)
- [ ] AI inference speed meets expectations
- [ ] Video processing progress is accurate

---

### 4. Compatibility Testing

#### 4.1 System Compatibility
- [ ] Windows 10 21H2 works
- [ ] Windows 10 22H2 works
- [ ] Windows 11 21H2 works
- [ ] Windows 11 22H2 works
- [ ] Windows 11 23H2 works

#### 4.2 Hardware Compatibility
- [ ] NVIDIA GPU works
- [ ] AMD GPU works
- [ ] Intel GPU works
- [ ] No GPU (CPU only) works

#### 4.3 Display Compatibility
- [ ] 1080p display works
- [ ] 1440p display works
- [ ] 4K display works
- [ ] High DPI scaling works
- [ ] Multi-monitor works

---

### 5. Documentation Checks

#### 5.1 Required Documents
- [ ] README.md updated
- [ ] CHANGELOG.md updated
- [ ] LICENSE file exists
- [ ] THIRD_PARTY_LICENSES.md updated
- [ ] CONTRIBUTING.md updated
- [ ] SECURITY.md updated

#### 5.2 Version Information
- [ ] CMakeLists.txt version number is correct
- [ ] CHANGELOG.md version entry exists
- [ ] README.md version number is correct (if applicable)

---

### 6. Packaging Checks

#### 6.1 Installer Checks
- [ ] NSIS installer generated successfully
- [ ] Portable ZIP generated successfully
- [ ] Installer size is reasonable (< 500MB)
- [ ] Installer runs normally
- [ ] Uninstaller runs normally

#### 6.2 File Integrity
- [ ] Executable file exists
- [ ] Qt DLL files are complete
- [ ] FFmpeg DLL files are complete
- [ ] AI model files exist
- [ ] Translation files exist
- [ ] LICENSE file exists
- [ ] THIRD_PARTY_LICENSES.md exists

#### 6.3 Installation Testing
- [ ] Fresh installation works
- [ ] Overwrite installation works
- [ ] First run after installation works
- [ ] No residual files after uninstall

---

### 7. Release Preparation

#### 7.1 Version Tag
- [ ] Git tag created
- [ ] Tag name format is correct (vX.Y.Z)
- [ ] Tag pushed successfully

#### 7.2 GitHub Release
- [ ] Release Notes prepared
- [ ] Installers uploaded
- [ ] Source archive generated
- [ ] Release status is Published

#### 7.3 Announcements
- [ ] Release announcement prepared
- [ ] Social media posts (if applicable)
- [ ] Project homepage updated

---

## 🔍 Post-Release Verification

### Auto-Update Check
- [ ] Application can detect new version
- [ ] Update prompt displays correctly

### User Feedback Channels
- [ ] GitHub Issues working
- [ ] Discussion forum working (if available)

### Monitoring
- [ ] Download statistics working
- [ ] Error report collection working (if available)

---

## 📝 Release Notes Template

```markdown
## EnhanceVision vX.Y.Z Release Notes

### Release Date
YYYY-MM-DD

### New Features
- Feature 1
- Feature 2

### Improvements
- Improvement 1
- Improvement 2

### Bug Fixes
- Fix 1
- Fix 2

### Known Issues
- Issue 1
- Issue 2

### Downloads
- Windows Installer: [EnhanceVision-vX.Y.Z-windows-x64-installer.exe]()
- Windows Portable: [EnhanceVision-vX.Y.Z-windows-x64-portable.zip]()

### Upgrade Notes
- Upgrade considerations
```

---

## ⚠️ FAQ

### Q: Do all compilation warnings need to be fixed?
A: It's recommended to fix all warnings. If some warnings cannot be fixed, document them in the release notes.

### Q: What test coverage is acceptable?
A: Core features must have 100% test pass rate. Edge features can be documented as known issues.

### Q: What if a bug is found after release?
A: Decide based on severity:
- Critical bug: Immediately release a patch version (PATCH)
- General bug: Record in Issues, fix in next version
- Minor issue: Record in known issues list

---

*Last updated: 2025*
