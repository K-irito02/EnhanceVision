# 嵌入式查看器标题栏光标修复

## 概述

**创建日期**: 2026-04-08  
**问题描述**: 在放大查看器嵌入式窗口中，当用户将鼠标悬停在窗口顶部的标题栏区域时，鼠标图标未能正确显示为预期的手掌抓握图标（用于表示可拖拽操作以将窗口转换为独立窗口）。

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `include/EnhanceVision/utils/WindowHelper.h` | 添加 `setOverrideCursor` 和 `restoreOverrideCursor` 方法声明 |
| `src/utils/WindowHelper.cpp` | 实现光标覆盖方法 |
| `qml/components/EmbeddedMediaViewer.qml` | 使用 C++ 方法设置光标 |

---

## 二、问题分析

### 尝试的方案

| 方案 | 描述 | 结果 |
|------|------|------|
| 方案1 | 调整外层 MouseArea 不覆盖标题栏（`anchors.topMargin`） | ❌ 未解决 |
| 方案2 | 为 titleDragArea 添加 `hoverEnabled: true` | ❌ 未解决 |
| 方案3 | 设置 titleDragArea 的 `z` 值高于 RowLayout | ❌ 未解决 |
| 方案4 | 使用 `HoverHandler` 代替 MouseArea 的 hoverEnabled | ❌ 未解决 |
| 方案5 | 使用专门的光标 MouseArea（`acceptedButtons: Qt.NoButton`） | ❌ 未解决 |
| 方案6 | 在 titleBar 级别添加 HoverHandler | ❌ 未解决 |
| 方案7 | 使用 C++ `QGuiApplication::setOverrideCursor()` | ✅ 成功 |

### 根本原因

Qt 6 在 Windows 上使用 `QQuickWidget` 时，QML 的 `cursorShape` 属性在某些嵌套组件场景下无法正确生效。日志显示 `containsMouse` 和 `cursorShape` 值都正确设置，但光标仍显示为默认箭头。这是 Qt 的光标管理机制在特定场景下的限制。

---

## 三、技术实现细节

### 解决方案

使用 C++ 端的 `QGuiApplication::setOverrideCursor()` 来强制设置光标，绕过 QML 的光标管理问题。

### 关键代码

#### WindowHelper.h
```cpp
Q_INVOKABLE void setOverrideCursor(int cursorShape);
Q_INVOKABLE void restoreOverrideCursor();
```

#### WindowHelper.cpp
```cpp
void WindowHelper::setOverrideCursor(int cursorShape)
{
    QGuiApplication::setOverrideCursor(QCursor(static_cast<Qt::CursorShape>(cursorShape)));
}

void WindowHelper::restoreOverrideCursor()
{
    QGuiApplication::restoreOverrideCursor();
}
```

#### EmbeddedMediaViewer.qml
```qml
MouseArea {
    id: titleDragArea
    hoverEnabled: true
    
    onContainsMouseChanged: {
        if (containsMouse) {
            WindowHelper.setOverrideCursor(pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
        } else {
            WindowHelper.restoreOverrideCursor()
        }
    }
    onPressedChanged: {
        if (containsMouse) {
            WindowHelper.restoreOverrideCursor()
            WindowHelper.setOverrideCursor(pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
        }
    }
}
```

---

## 四、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 悬停标题栏 | 显示手掌图标 | ✅ 通过 |
| 按下标题栏 | 显示抓握图标 | ✅ 通过 |
| 离开标题栏 | 恢复默认光标 | ✅ 通过 |
| 拖拽操作 | 光标正确切换 | ✅ 通过 |

---

## 五、经验总结

1. **QML cursorShape 的局限性**：在复杂的嵌套组件场景下，QML 的 `cursorShape` 可能无法正确生效，特别是在 Windows 上使用 `QQuickWidget` 时。

2. **C++ 层面的解决方案**：使用 `QGuiApplication::setOverrideCursor()` 是一个可靠的替代方案，它在应用程序级别强制设置光标。

3. **调试方法**：通过 `console.log` 输出 `containsMouse` 和 `cursorShape` 值，可以确认事件是否被正确捕获，从而定位问题是在事件层面还是光标渲染层面。
