# 窗口全屏/最大化/吸附显示缝隙漏光修复笔记

## 概述

修复 Windows 平台下无边框窗口在最大化、全屏、Aero Snap 吸附时四周出现缝隙漏光的问题。

**创建日期**: 2026-04-11

---

## 问题描述

| 窗口类型 | 拖拽全屏 | 最大化/全屏按钮 | 半屏/四分屏吸附 |
|----------|----------|----------------|----------------|
| 主程序界面 | ✅ 正常 | ✅ 正常 | ❌ 有缝隙 |
| 子窗口（独立式等） | ✅ 正常 | ❌ 有缝隙 | ❌ 有缝隙 |

**现象**：窗口四周有很小的缝隙，漏光。拖拽触发 Aero Snap 全屏时正常，但通过按钮触发最大化/全屏，或拖拽触发半屏/四分屏吸附时有缝隙。

---

## 根因分析

### 因素 1：Windows 不可见边框（WS_THICKFRAME）

项目所有窗口均使用 `Qt.FramelessWindowHint` + `WS_THICKFRAME`。`WS_THICKFRAME` 使 Windows 在窗口四周添加一个不可见边框（约 7-8px），用于调整大小手柄。

- **最大化时**：Windows 将窗口矩形扩展到屏幕外（超出工作区一个边框宽度），使边框隐藏在屏幕边缘之后。
- **吸附时**：窗口处于"正常"状态（非最大化），不可见边框在窗口矩形内可见，造成缝隙。

### 因素 2：DWM 帧扩展边距

两个辅助类均在初始化时调用 `DwmExtendFrameIntoClientArea(hwnd, {1,1,1,1})`，让 DWM 在客户区内绘制 1px 帧以启用窗口阴影。当窗口最大化/全屏/吸附时，这 1px 帧仍然绘制，形成可见缝隙。

### 因素 3：DWM 圆角偏好

两个辅助类均设置 `DWMWCP_ROUND` 圆角。Windows 11 上，吸附窗口的圆角在接触屏幕边缘的一侧产生小缝隙。

---

## 解决方案

### 核心策略

1. **动态调整 DWM 帧扩展边距**：根据窗口状态切换 `{1,1,1,1}`（正常，启用阴影）和 `{0,0,0,0}`（最大化/全屏/吸附，消除缝隙）
2. **动态调整 DWM 圆角偏好**：最大化/全屏时使用 `DWMWCP_DONOTROUND`，吸附窗口保持 `DWMWCP_ROUND`
3. **检测窗口"贴边"状态**：判断窗口是否接触显示器工作区边缘（覆盖半屏、三分屏、四分屏等所有吸附布局）

### 关键实现

#### 状态检测函数

```cpp
static bool isWindowMaximizedOrFullscreen(HWND hwnd)
{
    if (IsZoomed(hwnd))
        return true;

    RECT wr;
    GetWindowRect(hwnd, &wr);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(monitor, &mi);

    return wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top &&
           wr.right >= mi.rcMonitor.right && wr.bottom >= mi.rcMonitor.bottom;
}

static bool isWindowSnappedToEdge(HWND hwnd)
{
    if (IsZoomed(hwnd))
        return true;

    RECT wr;
    GetWindowRect(hwnd, &wr);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(monitor, &mi);

    // 全屏检测
    if (wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top &&
        wr.right >= mi.rcMonitor.right && wr.bottom >= mi.rcMonitor.bottom)
        return true;

    RECT wa = mi.rcWork;
    LONG workW = wa.right - wa.left;
    LONG workH = wa.bottom - wa.top;
    LONG winW = wr.right - wr.left;
    LONG winH = wr.bottom - wr.top;

    bool fillsWidth = (wr.left == wa.left && wr.right == wa.right) || (winW >= workW);
    bool fillsHeight = (wr.top == wa.top && wr.bottom == wa.bottom) || (winH >= workH);

    return fillsWidth || fillsHeight;
}
```

#### DWM 帧更新函数

```cpp
static void updateWindowFrame(HWND hwnd, bool removeMargins, bool squareCorners)
{
    MARGINS margins = removeMargins ? MARGINS{0, 0, 0, 0} : MARGINS{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    DWM_WINDOW_CORNER_PREFERENCE pref = squareCorners ? DWMWCP_DONOTROUND : DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
}
```

#### 状态更新调用

```cpp
void updateFrameForState()
{
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    bool maxOrFull = isWindowMaximizedOrFullscreen(hwnd);
    bool snapped = isWindowSnappedToEdge(hwnd);
    updateWindowFrame(hwnd, maxOrFull || snapped, maxOrFull);
}
```

---

## 遇到的问题及解决方案

### 问题 1：主窗口启动崩溃

**现象**：在 `WindowHelper::nativeEventFilter` 中添加 `WM_SIZE` 处理后，应用程序启动时崩溃。

**原因**：`QQuickWidget` 与原生窗口消息处理存在时序冲突。在 `nativeEventFilter` 中调用 DWM API 会干扰 Qt 内部的窗口初始化。

**解决方案**：
- 移除 `nativeEventFilter` 中的 `WM_SIZE` 处理
- 改用 Qt 事件系统，在 `eventFilter` 中处理 `QEvent::Resize`、`QEvent::Move`、`QEvent::WindowStateChange`
- 使用 `QTimer::singleShot(0, ...)` 延迟调用 DWM API，确保在事件循环空闲时执行

### 问题 2：吸附窗口圆角消失

**现象**：窗口吸附到屏幕边缘后，四个角变成直角，不再是圆角。

**原因**：初始实现将"贴边检测"和"圆角偏好"绑定在一起，任何贴边窗口都被设置为 `DWMWCP_DONOTROUND`。

**解决方案**：
- 分离"移除边距"和"设置直角"两个逻辑
- `removeMargins = maxOrFull || snapped`：任何贴边窗口都移除 1px DWM 边距
- `squareCorners = maxOrFull`：只有最大化/全屏窗口才使用直角，吸附窗口保持圆角

---

## 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `src/utils/WindowHelper.cpp` | 添加静态辅助函数、`updateFrameForState()` 方法，Qt 事件触发 DWM 更新 |
| `include/EnhanceVision/utils/WindowHelper.h` | 添加 `updateFrameForState()` 方法声明 |
| `src/utils/SubWindowHelper.cpp` | 添加静态辅助函数、`WM_SIZE` 处理、`updateFrameForState()` 方法 |
| `include/EnhanceVision/utils/SubWindowHelper.h` | 添加 `updateFrameForState()` 方法声明 |

---

## 测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 主窗口最大化按钮 | 无缝隙、直角 | ✅ 通过 |
| 主窗口拖拽全屏 | 无缝隙、直角 | ✅ 通过 |
| 主窗口半屏吸附 | 无缝隙、圆角 | ✅ 通过 |
| 主窗口四分屏吸附 | 无缝隙、圆角 | ✅ 通过 |
| 子窗口全屏按钮 | 无缝隙、直角 | ✅ 通过 |
| 子窗口拖拽全屏 | 无缝隙、直角 | ✅ 通过 |
| 子窗口半屏吸附 | 无缝隙、圆角 | ✅ 通过 |
| 正常窗口状态 | 有阴影、圆角 | ✅ 通过 |

---

## 技术要点

1. **Qt 事件 vs 原生消息**：`QQuickWidget` 主窗口使用 Qt 事件系统更安全，`QQuickWindow` 子窗口可以直接处理原生消息
2. **延迟执行**：使用 `QTimer::singleShot(0, ...)` 确保 DWM API 在事件循环空闲时调用
3. **状态分离**：将"移除边距"和"设置直角"分离，实现更精细的控制
4. **吸附检测**：通过比较窗口矩形与显示器工作区来判断吸附状态
