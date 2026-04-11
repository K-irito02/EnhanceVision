# 菜单UI优化实现笔记

## 概述

优化 MediaViewerControls 组件中的菜单样式，包括播放倍速菜单、播放设置菜单和音量菜单，使其在亮暗主题下正确显示，并修复图标资源缺失问题。

**创建日期**: 2026-04-11

---

## 变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/mediaViewer/MediaViewerControls.qml` | 优化菜单样式，支持亮暗主题 |
| `CMakeLists.txt` | 添加 Qt6::Svg 模块和缺失的图标资源 |

---

## 实现的功能特性

- ✅ 播放倍速菜单：删除选中项背景色和√图标，选中项显示蓝色字体，悬停显示淡蓝色字体
- ✅ 播放设置菜单：黑色背景白色字体，悬停淡蓝色，点击后菜单保持打开，中英文自适应宽度
- ✅ 音量菜单：支持亮暗主题，滑块和百分比颜色正确显示
- ✅ 图标资源：修复缺失的图标资源文件

---

## 技术实现细节

### 1. Qt.rgba 颜色值归一化

Qt.rgba 函数的参数范围是 0.0-1.0，而不是 0-255。需要将颜色值除以 255：

```qml
color: Theme.isDark ? Qt.rgba(25/255, 28/255, 38/255, 0.92) : Qt.rgba(255/255, 255/255, 255/255, 0.92)
```

### 2. Popup 代替 Menu

使用 Popup 代替 Menu 可以更好地控制布局和关闭行为：

```qml
Popup {
    id: settingsPopup
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // ...
}
```

### 3. 鼠标悬停检测

使用 MouseArea 检测悬停状态：

```qml
MouseArea {
    id: settingsHoverArea
    anchors.fill: parent
    hoverEnabled: true
    acceptedButtons: Qt.NoButton
}

property bool isHovered: settingsHoverArea.containsMouse
```

### 4. 中英文自适应宽度

根据当前语言设置不同的菜单宽度：

```qml
property bool isChinese: SettingsController.language.startsWith("zh")
property int menuWidth: isChinese ? 135 : 260
```

### 5. 图标资源添加

在 CMakeLists.txt 中添加缺失的图标资源：

```cmake
resources/icons/panel-right.svg
resources/icons/autoplay-on-open-on.svg
resources/icons/autoplay-on-open-off.svg
resources/icons/autoplay-on-switch-on.svg
resources/icons/autoplay-on-switch-off.svg
resources/icons/restore-position-on.svg
resources/icons/restore-position-off.svg
```

---

## 遇到的问题及解决方案

### 问题 1：Qt.rgba 颜色值错误

**现象**：颜色显示为白色或黑色，不是预期的颜色

**原因**：Qt.rgba 参数范围是 0.0-1.0，代码中使用了 0-255 的值

**解决方案**：将所有颜色值除以 255 进行归一化

### 问题 2：菜单点击后自动关闭

**现象**：点击播放设置菜单中的选项后菜单自动关闭

**原因**：MenuItem 的 onTriggered 会触发菜单关闭

**解决方案**：使用 ItemDelegate 代替 MenuItem，并设置 `closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside`

### 问题 3：图标不显示

**现象**：播放设置按钮和展开/收缩控制面板按钮的图标不显示

**原因**：图标资源文件没有被包含在 CMakeLists.txt 的 RESOURCES 部分

**解决方案**：在 CMakeLists.txt 中添加缺失的图标资源文件

---

## 测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 播放倍速菜单悬停 | 显示淡蓝色字体 | ✅ 通过 |
| 播放倍速菜单选中 | 显示蓝色字体，无背景色 | ✅ 通过 |
| 播放设置菜单悬停 | 显示淡蓝色字体 | ✅ 通过 |
| 播放设置菜单点击 | 菜单保持打开 | ✅ 通过 |
| 音量菜单亮暗主题 | 正确切换颜色 | ✅ 通过 |
| 图标显示 | 所有图标正常显示 | ✅ 通过 |
| 中英文菜单宽度 | 自适应宽度 | ✅ 通过 |

---

## 后续工作

- [ ] 无
