# UI Bug  fixes - 2026-04-08

## 1.  Sidebar Resize Cursor Fix

### 1.1  Problem Description
When moving the mouse to the right edge of the session sidebar, the cursor did not change to a horizontal double-headed arrow (cursorShape was incorrect).

### 1.2  Root Cause Analysis
1. **Cursor Shape Issue**: Used `Qt.SplitHCursor` instead of `Qt.SizeHorCursor`
2. **Event Interception**: `focusOverlay` MouseArea with high z-index was intercepting mouse events
3. **Clipping Issue**: Sidebar had `clip: true` causing resize handle to be clipped

### 1.3  Solution Implemented
1. **Fixed Cursor Shape**: Changed from `Qt.SplitHCursor` to `Qt.SizeHorCursor` in `App.qml`
2. **Event Propagation**: Modified `focusOverlay` MouseArea to propagate events:
   ```qml
   propagateComposedEvents: true
   mouse.accepted = false
   ```
3. **Moved Resize Handle**: Moved resize handle from inside Sidebar to App.qml to avoid clipping

### 1.4  Files Modified
| File | Changes |
|------|---------|
| `qml/App.qml` | Fixed cursor shape, adjusted focusOverlay, moved resize handle |
| `qml/components/Sidebar.qml` | Removed resize handle (moved to App.qml) |

---

## 2.  Processing Mode One-Way Control

### 2.1  Problem Description
The middle-bottom mode buttons (Shader mode, AI inference mode) should control the right-side control panel's mode switches, but the right-side control panel should NOT control the middle-bottom buttons (one-way control).

### 2.2  Root Cause Analysis
Both areas were directly binding to `UIStateController.processingMode`, creating two-way synchronization.

### 2.3  Solution Implemented
1. **Control Panel Local State**: Added local `displayMode` property in `ControlPanel.qml`
2. **One-Way Binding**: 
   - Middle buttons: Set `UIStateController.processingMode` on click
   - Control panel buttons: Only update local `displayMode` on click
3. **Sync Mechanism**: Added Connections to sync `displayMode` when `UIStateController.processingMode` changes

### 2.4  Technical Implementation
```qml
// ControlPanel.qml
property int displayMode: UIStateController.processingMode

// Sync from UIStateController to local displayMode
Connections {
    target: UIStateController
    function onProcessingModeChanged() {
        root.displayMode = UIStateController.processingMode
    }
}

// Control panel buttons only update local state
onClicked: root.displayMode = 0  // or 1
```

### 2.5  Files Modified
| File | Changes |
|------|---------|
| `qml/components/ControlPanel.qml` | Added local displayMode, one-way binding implementation |
| `qml/App.qml` | Removed conflicting processingMode binding override |

---

## 3.  Debug Information Cleanup

### 3.1  Problem Description
Found redundant console.log debug statements in QML files.

### 3.2  Solution Implemented
Removed console.log statements from:
- `qml/utils/ShortcutManager.qml`: Removed shortcut activation logs
- `qml/components/MessageItem.qml`: Removed time calculation debug log

### 3.3  Files Modified
| File | Changes |
|------|---------|
| `qml/utils/ShortcutManager.qml` | Removed console.log debug statements |
| `qml/components/MessageItem.qml` | Removed console.log debug statement |

---

## 4.  Runtime Warning Fixes

### 4.1  Problem Description
Runtime warnings detected:
1. `AIParamsPanel.qml:391:17`: `onContainsMouseChanged` signal handler mismatch
2. `AIParamsPanel.qml:392`: `_cpuMouseArea is not defined`

### 4.2  Solution Implemented
Added missing `id: _cpuMouseArea` to the MouseArea in CPU card.

### 4.3  Files Modified
| File | Changes |
|------|---------|
| `qml/components/AIParamsPanel.qml` | Added id to CPU MouseArea |

---

## 5.  Testing Verification

### 5.1  Test Cases
| Test Case | Expected Result | Status |
|-----------|----------------|--------|
| Sidebar resize cursor | Shows horizontal resize arrow on right edge |  Fixed |
| Middle button click | Updates control panel mode |  Fixed |
| Control panel click | Does NOT affect middle buttons |  Fixed |
| Runtime logs | No warnings or errors |  Fixed |

### 5.2  Build Verification
- Build completed successfully with only Qt deployment warning (expected)
- Application starts and runs without issues
- All UI interactions work as expected

---

## 6.  Technical Notes

### 6.1  QML Property Binding Best Practices
1. **Avoid Circular Bindings**: Be careful with two-way property bindings
2. **Use Connections for Sync**: Prefer Connections over signal handlers for cross-component sync
3. **Local State for UI**: Use local properties for UI-only state that doesn't need persistence

### 6.2  Event Handling in Layered UI
1. **Z-Order Matters**: Higher z-index elements can intercept events
2. **Event Propagation**: Use `propagateComposedEvents` and `mouse.accepted` carefully
3. **Clipping Effects**: `clip: true` affects both visual rendering and event handling

---

## 7.  Future Considerations

### 7.1  Potential Improvements
1. **Centralized Event Management**: Consider a centralized event system for complex UI interactions
2. **Property Binding Validation**: Add runtime checks for binding conflicts
3. **Debug Mode Toggle**: Implement a debug mode flag for conditional debug output

### 7.2  Maintenance Notes
- Monitor for similar binding conflicts in other UI components
- Regular audit of console.log statements in QML files
- Keep event handling patterns consistent across components
