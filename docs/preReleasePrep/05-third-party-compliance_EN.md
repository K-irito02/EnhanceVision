# Third-Party Dependency Compliance

English | [简体中文](05-third-party-compliance_CN.md)

This document details the third-party dependencies used by EnhanceVision and their license compliance requirements.

## 📋 Table of Contents

- [Dependency Overview](#dependency-overview)
- [License Compatibility Analysis](#license-compatibility-analysis)
- [Compliance Requirements](#compliance-requirements)
- [User Rights](#user-rights)
- [Compliance Checklist](#compliance-checklist)

---

## Dependency Overview

### Core Dependencies

| Dependency | Version | License | Linking Method | Required |
|------------|---------|---------|----------------|----------|
| Qt | 6.10.2 | LGPL v3 | Dynamic Linking | ✅ |
| NCNN | latest | BSD-3-Clause | Static Linking | ✅ |
| FFmpeg | 7.1 | LGPL v2.1+ | Dynamic Linking | ✅ |

### Optional Dependencies

| Dependency | Version | License | Linking Method | Required |
|------------|---------|---------|----------------|----------|
| OpenCV | Optional | Apache 2.0 | Static Linking | ❌ |
| Vulkan SDK | latest | Apache 2.0 | Dynamic Linking | ❌ |

### AI Models

| Model | Source | License | Purpose |
|-------|--------|---------|---------|
| Real-ESRGAN x4plus | xinntao/Real-ESRGAN | BSD-3-Clause | Image Super-Resolution |

---

## License Compatibility Analysis

### License Types

```
MIT (This Project)
    │
    ├── ✅ BSD-3-Clause (NCNN, Real-ESRGAN)
    │       Fully compatible, just retain copyright notice
    │
    ├── ✅ Apache 2.0 (OpenCV, Vulkan SDK)
    │       Compatible, retain copyright notice and NOTICE file
    │
    └── ✅ LGPL v2.1/v3 (Qt, FFmpeg)
            Compatible, but requires dynamic linking
```

### Compatibility Matrix

| License Combination | Compatibility | Conditions |
|---------------------|---------------|------------|
| MIT + BSD-3 | ✅ | Retain copyright notice |
| MIT + Apache 2.0 | ✅ | Retain copyright notice |
| MIT + LGPL (Dynamic) | ✅ | Dynamic linking, provide library source access |
| MIT + LGPL (Static) | ⚠️ | Must provide object files or source code |

---

## Compliance Requirements

### Qt (LGPL v3)

#### Requirements

1. **Dynamic Linking**
   - ✅ Met: This project dynamically links Qt libraries
   - Users can replace Qt library versions

2. **Provide Source Access Method**
   - Document Qt source download location
   - Include LGPL license text in installation package

3. **Copyright Notice**
   - Display Qt copyright information in About dialog
   - Declare in THIRD_PARTY_LICENSES.md

#### Implementation

**Add About dialog in application:**

```qml
// qml/pages/AboutPage.qml
Dialog {
    title: qsTr("About EnhanceVision")
    
    Column {
        Text {
            text: "EnhanceVision v0.1.0"
        }
        Text {
            text: qsTr("Built with Qt 6.10.2")
        }
        Text {
            text: qsTr("Qt is licensed under LGPL v3")
            link: "https://www.qt.io/licensing"
        }
    }
}
```

**Declare in documentation:**

```markdown
## Qt Framework

This application uses Qt Framework, licensed under GNU Lesser General Public License v3.0 (LGPL v3).

- Qt source code is available at https://download.qt.io/official_releases/qt/
- Full LGPL v3 license text is available at https://www.gnu.org/licenses/lgpl-3.0.html
- Users can replace the Qt libraries bundled with this application with modified versions
```

### FFmpeg (LGPL v2.1+)

#### Requirements

1. **Use LGPL Version**
   - ✅ Met: Using FFmpeg LGPL version
   - Not using GPL components (like libx264)

2. **Dynamic Linking**
   - ✅ Met: Dynamically linking FFmpeg libraries

3. **Provide Source Access Method**
   - Document FFmpeg source download location

#### Implementation

**Verify FFmpeg configuration:**

```powershell
# Check FFmpeg configuration
ffmpeg -version

# Confirm no GPL components
# Output should include: --enable-lgpl
# Should not include: --enable-gpl
```

**Declare in documentation:**

```markdown
## FFmpeg

This application uses FFmpeg, licensed under GNU Lesser General Public License v2.1 or later.

- FFmpeg source code is available at https://ffmpeg.org/releases/
- Full LGPL v2.1 license text is available at https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
```

### NCNN (BSD-3-Clause)

#### Requirements

1. **Retain Copyright Notice**
   - Include full license in THIRD_PARTY_LICENSES.md

2. **No Endorsement**
   - Cannot use "NCNN" or "Tencent" for promotion without permission

#### Implementation

Already declared in THIRD_PARTY_LICENSES.md.

### Real-ESRGAN (BSD-3-Clause)

#### Requirements

1. **Retain Copyright Notice**
   - Include copyright information in model file documentation

2. **Follow Usage Restrictions**
   - Only for legitimate purposes

#### Implementation

**Add README in models directory:**

```markdown
# AI Models

This directory contains AI models for image enhancement.

## Real-ESRGAN x4plus

- Source: https://github.com/xinntao/Real-ESRGAN
- License: BSD 3-Clause License
- Copyright: Copyright (c) 2021, Xintao Wang

Using this model indicates your agreement to comply with its license terms.
```

---

## User Rights

### Under LGPL License, users have the following rights:

#### 1. Right to Replace Library Files

Users can replace the libraries bundled with the application with modified versions.

**Replacement Steps:**

1. Download or compile Qt/FFmpeg from official sources
2. Replace DLL files in application directory
3. Restart application

#### 2. Right to Relink

Users can relink the application with different versions of the libraries.

**Technical Notes:**
- Application dynamically links Qt and FFmpeg
- Users can replace DLL files to achieve relinking
- No need to recompile the application

#### 3. Right to Access Source Code

Users have the right to access the source code of LGPL-licensed libraries.

**Access Methods:**
- Qt: https://download.qt.io/official_releases/qt/
- FFmpeg: https://ffmpeg.org/releases/

---

## Compliance Checklist

### Pre-Release Checks

- [ ] **Qt Compliance**
  - [ ] Dynamic linking to Qt libraries
  - [ ] Installation package includes Qt LGPL license text
  - [ ] Documentation explains Qt source access method
  - [ ] About dialog displays Qt copyright information

- [ ] **FFmpeg Compliance**
  - [ ] Using LGPL version (not GPL)
  - [ ] Dynamic linking to FFmpeg libraries
  - [ ] Documentation explains FFmpeg source access method

- [ ] **NCNN Compliance**
  - [ ] THIRD_PARTY_LICENSES.md includes BSD-3 license
  - [ ] Copyright notice retained

- [ ] **Real-ESRGAN Compliance**
  - [ ] Model directory includes copyright notice
  - [ ] THIRD_PARTY_LICENSES.md includes license

### Installation Package Checks

- [ ] Contains LICENSE file (MIT)
- [ ] Contains THIRD_PARTY_LICENSES.md
- [ ] Qt DLL files are dynamically linked
- [ ] FFmpeg DLL files are dynamically linked
- [ ] Model files include copyright notice

### Documentation Checks

- [ ] README.md includes license information
- [ ] THIRD_PARTY_LICENSES.md is complete
- [ ] About dialog shows third-party copyrights

---

## Legal Notice Template

### Application Splash Screen/About Dialog

```
EnhanceVision v0.1.0
Copyright © 2025 EnhanceVision Contributors

This software uses the following open-source components:
- Qt Framework (LGPL v3)
- NCNN (BSD-3-Clause)
- FFmpeg (LGPL v2.1+)

See THIRD_PARTY_LICENSES.md for details
```

### Installer License Agreement Page

```
End User License Agreement

EnhanceVision is licensed under the MIT License.

This application contains third-party components licensed under LGPL (Qt, FFmpeg).
These components are dynamically linked, and you can replace them.

For complete license information, see:
- LICENSE (MIT License)
- THIRD_PARTY_LICENSES.md (Third-Party Licenses)

[I Agree] [I Disagree]
```

---

## FAQ

### Q: Why choose MIT License?

A: MIT is one of the most permissive open-source licenses, allowing users to freely use, modify, and distribute the code with only a copyright notice. This aligns with our open-source philosophy.

### Q: Does LGPL require me to open-source my code?

A: No. As long as you dynamically link to LGPL libraries, your application code can remain closed-source. Users only have the right to replace the LGPL libraries themselves.

### Q: What if I want to statically link Qt?

A: Static linking to LGPL libraries requires:
1. Providing object files (.obj) for user relinking
2. Or purchasing a Qt commercial license

We chose dynamic linking to avoid this complexity.

### Q: How can users replace Qt libraries?

A: Users can:
1. Download or compile a new version of Qt
2. Replace Qt*.dll files in the application directory
3. Restart the application

---

## References

- [Qt Licensing](https://www.qt.io/licensing)
- [FFmpeg License and Legal Considerations](https://ffmpeg.org/legal.html)
- [Choose a License](https://choosealicense.com/)
- [Open Source Initiative Licenses](https://opensource.org/licenses)

---

*Last updated: 2025*
