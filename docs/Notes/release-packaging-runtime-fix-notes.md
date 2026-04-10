# Release Packaging and Runtime Fix Notes

## Overview

Fixed several release-only issues in the packaged application and aligned the packaging workflow with the actual runtime deployment requirements.

**Date**: 2026-04-10

---

## Changes

### Updated Files

| File | Change |
|------|--------|
| `qml/controls/RichTooltip.qml` | Stabilized overlay-relative tooltip positioning |
| `qml/controls/Tooltip.qml` | Stabilized overlay-relative tooltip positioning |
| `qml/components/MessageItem.qml` | Relaxed timing visibility conditions |
| `src/core/ProcessingTimeManager.cpp` | Synced timing state back to `MessageModel` |
| `qml/components/EmbeddedMediaViewer.qml` | Normalized local video source URLs |
| `qml/components/MediaViewerWindow.qml` | Normalized local video source URLs |
| `qml/components/OffscreenShaderRenderer.qml` | Normalized source URLs for export |
| `qml/controls/Button.qml` | Added localized color overrides for soft destructive buttons |
| `qml/pages/SettingsPage.qml` | Softened the `清理全部` button styling |
| `src/main.cpp` | Moved runtime logs into app-local data storage |
| `scripts/build-package.ps1` | Added `windeployqt` deployment step |
| `scripts/verify-package.ps1` | Verified package contents against the release runtime tree |
| `installer/setup.nsi` | Simplified installer file copy to mirror the package output |
| `docs/preReleasePrep/03-packaging-guide_CN.md` | Updated Chinese packaging instructions to match the deployed runtime layout |
| `docs/preReleasePrep/03-packaging-guide_EN.md` | Updated English packaging instructions to match the deployed runtime layout |
| `.agents/skills/packaging-workflow/SKILL.md` | Synced the primary packaging skill with the current release flow |
| `.trae/skills/packaging-workflow/SKILL.md` | Synced the alternate packaging skill entry with the current release flow |

---

## Key Outcomes

- Release builds now load Qt Multimedia backends correctly.
- Packaged video playback works again in the enlarged viewer.
- Message cards show predicted and total timing data consistently.
- Tooltip positioning is more stable near window edges.
- Packaging instructions now match the actual generated runtime layout.

---

## Verification

- Release build completed successfully.
- Package generation completed successfully.
- Packaged executable launched and loaded `ffmpegmediaplugin.dll` successfully.
- Runtime log now writes to the user-local app data directory.
