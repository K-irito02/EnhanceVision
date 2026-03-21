# 设置记忆功能修复

## 概述

本次修复解决了应用程序设置（主题、语言、音量）无法正确记忆的问题。修复了多个位置（设置页面、标题栏、查看器窗口）的设置保存逻辑，确保用户在关闭应用程序后，下次打开时设置保持不变。

**创建日期**: 2026-03-21
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 设置控制器立即保存

| 文件 | 修改内容 |
|------|---------|
| `src/controllers/SettingsController.cpp` | `setTheme()`、`setLanguage()`、`setVolume()` 添加 `saveSettings()` 立即保存 |

### 2. 主题切换保存

| 文件 | 修改内容 |
|------|---------|
| `qml/styles/Theme.qml` | `toggle()` 和 `setDark()` 添加 `SettingsController.theme = ...` 保存主题 |
| `qml/styles/Theme.qml` | `Component.onCompleted` 中加载主题设置 |

### 3. 语言切换保存

| 文件 | 修改内容 |
|------|---------|
| `qml/pages/SettingsPage.qml` | 语言切换时添加 `SettingsController.language = lang` |

### 4. 音量调节保存

| 文件 | 修改内容 |
|------|---------|
| `qml/components/EmbeddedMediaViewer.qml` | 静音按钮和音量滑块添加 `SettingsController.volume = ...` |
| `qml/components/MediaViewerWindow.qml` | 静音按钮和音量滑块添加 `SettingsController.volume = ...` |

### 5. 实现的功能特性

- ✅ 设置页面修改后立即保存
- ✅ 标题栏主题切换后立即保存
- ✅ 标题栏语言切换后立即保存
- ✅ 查看器窗口音量调节后立即保存
- ✅ 静音按钮点击后立即保存
- ✅ 应用启动时正确加载所有设置

---

## 二、遇到的问题及解决方案

### 问题 1：设置只在应用退出时保存

**现象**：设置修改后，如果应用异常退出或崩溃，设置会丢失

**原因**：`SettingsController` 只在析构函数中调用 `saveSettings()`

**解决方案**：在 `setTheme()`、`setLanguage()`、`setVolume()` 方法中，值改变后立即调用 `saveSettings()`

### 问题 2：主题切换没有保存

**现象**：通过标题栏切换主题后重启应用，主题恢复为默认值

**原因**：`Theme.toggle()` 和 `Theme.setDark()` 只更新了 `isDark` 属性，没有保存到 `SettingsController`

**解决方案**：在 `toggle()` 和 `setDark()` 中添加 `SettingsController.theme = isDark ? "dark" : "light"`

### 问题 3：主题设置启动时没有加载

**现象**：应用启动时总是使用默认主题，不加载保存的主题

**原因**：`Theme.qml` 的 `Component.onCompleted` 中只加载了语言设置，没有加载主题设置

**解决方案**：在 `Component.onCompleted` 中添加 `isDark = (SettingsController.theme === "dark")`

### 问题 4：语言切换没有保存

**现象**：通过设置页面切换语言后重启应用，语言恢复为默认值

**原因**：`SettingsPage.qml` 中语言切换时只调用了 `Theme.setLanguage(lang)`，没有保存到 `SettingsController`

**解决方案**：在语言切换时添加 `SettingsController.language = lang`

### 问题 5：音量调节没有保存

**现象**：在查看器窗口调节音量后重启应用，音量恢复为默认值

**原因**：音量滑块和静音按钮没有保存到 `SettingsController.volume`

**解决方案**：
- 音量滑块 `onMoved` 时添加 `SettingsController.volume = Math.round(value * 100)`
- 静音按钮点击时添加 `SettingsController.volume = 0`

### 问题 6：静音按钮记忆失效

**现象**：点击静音按钮后重启应用，音量没有保持为 0

**原因**：静音按钮只更新了 `audioOutput.volume` 或 `sharedVolume`，没有保存到 `SettingsController.volume`

**解决方案**：在静音按钮的 `onClicked` 中添加 `SettingsController.volume = 0`

---

## 三、技术实现细节

### 设置保存流程

```
用户操作（切换主题/语言/音量）
    ↓
更新 UI 状态
    ↓
保存到 SettingsController（内存）
    ↓
调用 saveSettings()（立即保存到文件）
    ↓
写入 %LocalAppData%/EnhanceVision/settings.ini
```

### 设置加载流程

```
应用启动
    ↓
SettingsController 构造函数
    ↓
loadSettings() 从 settings.ini 加载
    ↓
Theme.qml Component.onCompleted
    ↓
应用保存的主题和语言设置
```

### 关键代码片段

**SettingsController 立即保存**:
```cpp
void SettingsController::setTheme(const QString& theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        emit themeChanged();
        emit settingsChanged();
        saveSettings();  // 立即保存
    }
}
```

**Theme 切换保存**:
```qml
function toggle() {
    isDark = !isDark
    SettingsController.theme = isDark ? "dark" : "light"
}
```

**音量调节保存**:
```qml
Slider {
    onMoved: {
        root.sharedVolume = value
        SettingsController.volume = Math.round(value * 100)
    }
}
```

**静音按钮保存**:
```qml
IconButton {
    onClicked: {
        if (root.sharedVolume > 0) {
            root.sharedVolume = 0
            SettingsController.volume = 0  // 保存静音状态
        } else {
            root.sharedVolume = SettingsController.volume / 100
        }
    }
}
```

---

## 四、修改的文件清单

| 文件 | 修改类型 | 修改内容 |
|------|---------|---------|
| `src/controllers/SettingsController.cpp` | 修改 | `setTheme()`、`setLanguage()`、`setVolume()` 添加 `saveSettings()` |
| `qml/styles/Theme.qml` | 修改 | `toggle()` 和 `setDark()` 保存主题，启动时加载主题 |
| `qml/pages/SettingsPage.qml` | 修改 | 语言切换时保存到 `SettingsController` |
| `qml/components/EmbeddedMediaViewer.qml` | 修改 | 静音按钮和音量滑块保存音量 |
| `qml/components/MediaViewerWindow.qml` | 修改 | 静音按钮和音量滑块保存音量 |
