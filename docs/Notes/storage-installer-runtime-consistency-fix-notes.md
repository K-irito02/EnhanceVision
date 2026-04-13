# Storage Installer Runtime Consistency Fix Notes

## Overview

This note records the storage-directory, installer-language, and elevated-launch drag-and-drop fixes completed on 2026-04-14.

The goal was to eliminate mismatches between installer-selected paths and runtime-visible paths, while preserving predictable Windows behavior for configuration data, exported files, and elevated startup.

## Key Changes

### Storage directory unification

- Unified runtime storage resolution around `SettingsController`
- Continued using:
  - `behavior/customDataPath`
  - `behavior/defaultSavePath`
- Aligned installer writes with runtime reads so the Settings page shows the actual effective paths
- Moved runtime logs and crash logs to `{effectiveDataPath}/logs`

### Legacy data handling

- Removed the old reset affordance in the Settings page
- Added conditional `Migrate` and `Cleanup` actions that appear only when a real previous data directory exists
- Migration is conflict-aware:
  - current files are preserved
  - incoming legacy conflicts are copied with `_legacy_<timestamp>` suffixes
- After migration, session-persisted result paths are remapped from old storage root to new storage root
- Thumbnail provider and thumbnail database state are refreshed after migration to avoid UI/path conflicts

### Installer and first-run behavior

- Installer now places install directory and default export path on the same setup page
- Application data directory is derived from `InstallDir\data`
- Protected install directories trigger warnings instead of forced changes
- First launch now inherits the installer-selected language if no saved runtime language exists yet

### Elevated launch drag-and-drop compatibility

- On Windows, when the app is launched from an elevated installer finish page, Explorer drag-and-drop can be blocked by UIPI
- `main.cpp` now enables the required message filters for:
  - `WM_DROPFILES`
  - `WM_COPYDATA`
  - `WM_COPYGLOBALDATA`

## Files Touched

| File | Purpose |
|------|---------|
| `packaging/installer/setup.nsi` | Installer storage page, language persistence, path formatting, protected-dir warning |
| `src/controllers/SettingsController.cpp` | Unified storage resolution, legacy migrate/cleanup, runtime path fallback |
| `src/controllers/SessionController.cpp` | Remap persisted storage-root paths after legacy migration |
| `src/main.cpp` | Startup language fallback, runtime log path init, elevated drag/drop compatibility |
| `qml/pages/SettingsPage.qml` | Storage UI, migrate/cleanup buttons, bilingual tooltip content |
| `docs/ApplicationData/StorageDirectory.md` | Updated storage design and actual runtime behavior |

## Log Review

Latest checked logs in `logs/` did not show a new blocking regression from these changes.

Observed warnings were explainable:

- `Abnormal exit detected` came from an intentional test run that killed the process
- `Retrying to obtain clipboard` is an environment/runtime warning and not specific to this storage fix set

No new persistent startup translation warnings or storage-resolution warnings were found in the checked logs.

## Verification

- Release target compiled successfully
- Startup path handling compiled and linked successfully
- Drag-and-drop compatibility patch compiled successfully
- Documentation/rules were updated to match the implemented behavior

## Residual Risks

- Installing into protected locations is still user-allowed, so some permission-sensitive workflows may remain environment-dependent
- `sessions.json` still lives in the Windows-managed config area by design; only large runtime artifacts are redirected into the application data directory
