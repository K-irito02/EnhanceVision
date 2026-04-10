# 窗口全屏/最大化/吸附显示缝隙漏光修复计划

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

- **最大化时**：Windows 将窗口矩形扩展到屏幕外（超出工作区一个边框宽度），使边框隐藏在屏幕边缘之后。但如果 `WM_NCCALCSIZE` 处理不当，客户区不会正确填满可见区域。
- **吸附时**：窗口处于"正常"状态（非最大化），不可见边框在窗口矩形内可见，造成缝隙。

### 因素 2：DWM 帧扩展边距

两个辅助类均在初始化时调用 `DwmExtendFrameIntoClientArea(hwnd, {1,1,1,1})`，让 DWM 在客户区内绘制 1px 帧以启用窗口阴影。当窗口最大化/全屏/吸附时，这 1px 帧仍然绘制，形成可见缝隙。

### 因素 3：DWM 圆角偏好

两个辅助类均设置 `DWMWCP_ROUND` 圆角。Windows 11 上，吸附窗口的圆角在接触屏幕边缘的一侧产生小缝隙。

### 因素 4：WM_NCCALCSIZE 处理差异

| 辅助类 | WM_NCCALCSIZE 处理 | 效果 |
|--------|-------------------|------|
| WindowHelper（主窗口） | **无处理**，依赖 Qt 默认行为 | 最大化正常（Qt 正确处理），吸附异常 |
| SubWindowHelper（子窗口） | `wParam==TRUE` 时返回 0 | 理论上客户区=窗口矩形，但最大化/全屏时 DWM 边距仍造成缝隙 |

---

## 修复方案

### 核心策略

1. **正确处理 `WM_NCCALCSIZE`**：最大化时将客户区设为显示器工作区，消除不可见边框
2. **动态调整 DWM 帧扩展边距**：根据窗口状态切换 `{1,1,1,1}`（正常，启用阴影）和 `{0,0,0,0}`（最大化/全屏/吸附，消除缝隙）
3. **动态调整 DWM 圆角偏好**：最大化/全屏/吸附时使用 `DWMWCP_DONOTROUND`
4. **检测窗口"贴边"状态**：判断窗口是否接触显示器工作区边缘（覆盖半屏、三分屏、四分屏等所有吸附布局）

### 检测"贴边"状态的逻辑

```cpp
// 判断窗口是否应该移除边框（最大化/全屏/贴边吸附）
static bool shouldRemoveWindowBorder(HWND hwnd)
{
    // 1. 最大化状态
    if (IsZoomed(hwnd)) return true;

    RECT wr;
    GetWindowRect(hwnd, &wr);

    // 2. 全屏状态（窗口矩形覆盖整个显示器）
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(monitor, &mi);
    if (wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top &&
        wr.right >= mi.rcMonitor.right && wr.bottom >= mi.rcMonitor.bottom)
        return true;

    // 3. 贴边吸附（窗口至少一条边对齐显示器工作区边缘）
    RECT wa = mi.rcWork;
    bool touchLeft   = wr.left == wa.left;
    bool touchRight  = wr.right == wa.right;
    bool touchTop    = wr.top == wa.top;
    bool touchBottom = wr.bottom == wa.bottom;
    return touchLeft || touchRight || touchTop || touchBottom;
}
```

### 动态 DWM 边距/圆角更新

```cpp
static void updateWindowFrame(HWND hwnd, bool borderless)
{
    MARGINS margins = borderless ? MARGINS{0,0,0,0} : MARGINS{1,1,1,1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    DWM_WINDOW_CORNER_PREFERENCE pref = borderless ? DWMWCP_DONOTROUND : DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
}
```

---

## 实施步骤

### 步骤 1：修改 WindowHelper（主窗口）

**文件**：`src/utils/WindowHelper.cpp`、`include/EnhanceVision/utils/WindowHelper.h`

1. 在 `nativeEventFilter` 中添加 `WM_NCCALCSIZE` 处理：
   - `wParam==TRUE` 且窗口最大化时：将 `rgrc[0]` 设为显示器工作区
   - `wParam==TRUE` 且窗口非最大化时：返回 0（客户区=窗口矩形）

2. 在 `nativeEventFilter` 中添加 `WM_SIZE` 处理：
   - `SIZE_MAXIMIZED`：调用 `updateWindowFrame(hwnd, true)`
   - `SIZE_RESTORED`：调用 `updateWindowFrame(hwnd, shouldRemoveWindowBorder(hwnd))`

3. 在 `eventFilter` 中处理 `QEvent::WindowStateChange`：
   - 检测窗口状态变化，更新 DWM 边距和圆角

4. 添加私有方法 `updateFrameForState()` 封装 DWM 边距/圆角更新逻辑

### 步骤 2：修改 SubWindowHelper（子窗口）

**文件**：`src/utils/SubWindowHelper.cpp`、`include/EnhanceVision/utils/SubWindowHelper.h`

1. 修复 `SubWindowWndProc` 中的 `WM_NCCALCSIZE` 处理：
   - `wParam==TRUE` 且窗口最大化时：将 `rgrc[0]` 设为显示器工作区
   - `wParam==TRUE` 且窗口非最大化时：返回 0

2. 在 `SubWindowWndProc` 中添加 `WM_SIZE` 处理：
   - `SIZE_MAXIMIZED`：调用 `updateWindowFrame(hwnd, true)`
   - `SIZE_RESTORED`：调用 `updateWindowFrame(hwnd, shouldRemoveWindowBorder(hwnd))`

3. 在 `eventFilter` 中处理 `QEvent::WindowStateChange`：
   - 检测全屏状态变化（`showFullScreen()` 不触发 `WM_SIZE` 的 `SIZE_MAXIMIZED`）
   - 全屏时：调用 `updateWindowFrame(hwnd, true)`
   - 退出全屏时：调用 `updateWindowFrame(hwnd, shouldRemoveWindowBorder(hwnd))`

4. 添加私有方法 `updateFrameForState()` 封装 DWM 边距/圆角更新逻辑

### 步骤 3：提取共享逻辑

为减少代码重复，将以下逻辑提取为文件级静态函数（在各自的 .cpp 文件中）：

- `shouldRemoveWindowBorder(HWND hwnd)` - 判断是否应移除边框
- `updateWindowFrame(HWND hwnd, bool borderless)` - 更新 DWM 边距和圆角

由于 WindowHelper 和 SubWindowHelper 使用不同的消息处理机制（原生事件过滤器 vs 窗口过程子类化），共享逻辑以静态函数形式内联在各自的 .cpp 文件中，避免引入额外类。

### 步骤 4：构建验证

使用 `qt-build-and-fix` 技能构建项目，确保编译通过。

### 步骤 5：运行验证

运行应用程序，测试以下场景：
- 主窗口：最大化按钮、拖拽全屏、半屏吸附、四分屏吸附
- 子窗口：全屏按钮、拖拽全屏、半屏吸附、四分屏吸附
- 从最大化/全屏拖出恢复
- 正常状态下窗口阴影是否正常

---

## 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `src/utils/WindowHelper.cpp` | 添加 WM_NCCALCSIZE、WM_SIZE 处理，动态 DWM 边距/圆角 |
| `include/EnhanceVision/utils/WindowHelper.h` | 添加 updateFrameForState() 方法声明 |
| `src/utils/SubWindowHelper.cpp` | 修复 WM_NCCALCSIZE，添加 WM_SIZE 处理，动态 DWM 边距/圆角 |
| `include/EnhanceVision/utils/SubWindowHelper.h` | 添加 updateFrameForState() 方法声明 |

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| DWM 边距动态切换可能导致闪烁 | 低 | 使用 `SWP_FRAMECHANGED` 强制刷新，避免多次调用 |
| 贴边检测误判（窗口恰好在屏幕边缘） | 低 | 即使误判，仅影响阴影显示，不影响功能 |
| Qt 内部 WM_NCCALCSIZE 处理被覆盖 | 中 | 仅在最大化时覆盖，正常状态返回 0 不影响 Qt 行为 |
| 高 DPI 下边框宽度不同 | 低 | 使用 `GetSystemMetrics(SM_CXFRAME)` 获取实际边框宽度 |
| Windows 10/11 行为差异 | 低 | 方案基于 Win32 API，两个系统均兼容 |
