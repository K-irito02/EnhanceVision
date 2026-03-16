# 自定义标题栏统一实现笔记

## 概述

本文档记录了将项目中所有使用原生 Windows 标题栏的窗口改为自定义标题栏的实现过程，包括遇到的问题和解决方案。

**创建日期**: 2026-03-16  
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 创建的新组件

| 组件 | 文件路径 | 功能描述 |
|------|---------|---------|
| DialogTitleBar | `qml/components/DialogTitleBar.qml` | 可复用的对话框标题栏组件，支持拖拽、关闭按钮 |
| SubWindowHelper | `include/EnhanceVision/utils/SubWindowHelper.h` | 子窗口辅助类头文件 |
| SubWindowHelper | `src/utils/SubWindowHelper.cpp` | 子窗口辅助类实现，处理 Windows 原生事件 |

### 2. 修改的窗口

| 窗口 | 文件 | 功能 |
|------|------|------|
| 媒体查看器窗口 | `qml/components/MediaViewerWindow.qml` | 自定义标题栏、圆角、四周拉伸、Windows Snap |
| 删除类别对话框 | `qml/components/ShaderParamsPanel.qml` | 自定义标题栏 |
| 新建类别对话框 | `qml/components/ShaderParamsPanel.qml` | 自定义标题栏 |
| 重命名类别对话框 | `qml/components/ShaderParamsPanel.qml` | 自定义标题栏 |
| 重命名风格对话框 | `qml/components/ShaderParamsPanel.qml` | 自定义标题栏 |
| 保存风格对话框 | `qml/components/ShaderParamsPanel.qml` | 自定义标题栏 |

### 3. 实现的功能特性

- ✅ 所有窗口使用统一的自定义标题栏
- ✅ 标题栏样式与主题一致
- ✅ 支持拖拽移动窗口
- ✅ 支持最小化、最大化、关闭
- ✅ 窗口有圆角效果
- ✅ 四周自由拉伸调整大小
- ✅ Windows Snap 功能吸附
- ✅ 最小窗口尺寸限制
- ✅ 按钮区域排除（避免拖拽拦截点击）

---

## 二、遇到的问题及解决方案

### 问题 1：按钮需要点击两次才起作用

**现象**：标题栏上的关闭、全屏等按钮需要点击两次才能触发。

**原因**：`MouseArea` 定义在按钮之后，由于 QML 中后定义的元素会覆盖先定义的元素（z-order），导致 `MouseArea` 覆盖在按钮上面，拦截了按钮的点击事件。第一次点击被 `MouseArea` 拦截，第二次点击才能传递到按钮。

**解决方案**：通过设置 `z` 属性调整层级：
```qml
MouseArea {
    z: 0  // 底层
    // ...
}

RowLayout {
    z: 1  // 中间层
    // 按钮...
}
```

### 问题 2：多个子窗口无法同时使用原生事件处理

**现象**：使用 `QAbstractNativeEventFilter` 时，多个 `SubWindowHelper` 实例会冲突。

**原因**：`QAbstractNativeEventFilter` 是全局的，每个实例注册后都会收到所有窗口的消息，无法区分是哪个窗口的消息。

**解决方案**：改用 Windows 子类化技术（`SetWindowLongPtr` + `GWLP_WNDPROC`）：
```cpp
// 使用 QHash 存储窗口句柄与 Helper 的映射
static QHash<HWND, SubWindowHelper*> g_windowHelpers;

// 子类化窗口过程
static LRESULT CALLBACK SubWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto it = g_windowHelpers.find(hwnd);
    if (it != g_windowHelpers.end()) {
        SubWindowHelper* helper = it.value();
        // 处理消息...
    }
    return CallWindowProc(g_originalQtWndProc, hwnd, msg, wParam, lParam);
}
```

### 问题 3：全局函数无法访问类的私有成员

**现象**：编译错误，全局回调函数无法访问 `SubWindowHelper` 的私有成员。

**原因**：C++ 友元函数声明需要正确的函数签名。

**解决方案**：
1. 在头文件中声明友元函数：
```cpp
#ifdef Q_OS_WIN
friend LRESULT CALLBACK SubWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
```

2. 添加公有访问函数：
```cpp
const QVector<QRect>& excludeRegions() const { return m_excludeRegions; }
bool isWindowMaximized() const { return m_isMaximized; }
int windowResizeMargin() const { return m_resizeMargin; }
int windowMinWidth() const { return m_minWidth; }
int windowMinHeight() const { return m_minHeight; }
```

### 问题 4：窗口定位问题

**现象**：对话框窗口无法正确居中显示。

**原因**：在 QML `Window` 中，`Window.window` 只对 `Item` 有效，不能直接获取父窗口。

**解决方案**：使用 `root.Window.window` 从根组件获取窗口引用：
```qml
onVisibleChanged: {
    if (visible) {
        var mainWindow = root.Window.window
        if (mainWindow) {
            x = mainWindow.x + (mainWindow.width - width) / 2
            y = mainWindow.y + (mainWindow.height - height) / 2
        }
    }
}
```

---

## 三、技术实现细节

### 1. Windows 原生消息处理

通过 `WM_NCHITTEST` 消息判断鼠标位置，返回对应的窗口区域标识：

| 返回值 | 含义 |
|--------|------|
| `HTCAPTION` | 标题栏区域，支持拖拽 |
| `HTLEFT/HTRIGHT` | 左/右边框，支持水平调整大小 |
| `HTTOP/HTBOTTOM` | 上/下边框，支持垂直调整大小 |
| `HTTOPLEFT/HTTOPRIGHT` | 左上/右上角，支持对角调整大小 |
| `HTBOTTOMLEFT/HTBOTTOMRIGHT` | 左下/右下角，支持对角调整大小 |

### 2. 圆角窗口实现

使用 DWM API 设置窗口圆角：
```cpp
DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
```

### 3. 排除区域机制

为了避免标题栏按钮被拖拽区域拦截，实现了排除区域机制：
```cpp
Q_INVOKABLE void clearExcludeRegions();
Q_INVOKABLE void addExcludeRegion(int x, int y, int width, int height);
```

在 QML 中动态更新排除区域：
```qml
function updateExcludeRegions() {
    windowHelper.clearExcludeRegions()
    var buttonsRow = titleBarButtonsRow
    if (buttonsRow && buttonsRow.width > 0) {
        var globalPos = buttonsRow.mapToItem(titleBar, 0, 0)
        windowHelper.addExcludeRegion(globalPos.x, globalPos.y, buttonsRow.width, buttonsRow.height)
    }
}
```

---

## 四、架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                      窗口层级架构                             │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────┐     ┌─────────────────────────────┐   │
│  │   MainWindow    │     │      SubWindow (QML)        │   │
│  │  (QQuickWidget) │     │  ┌───────────────────────┐  │   │
│  │                 │     │  │  DialogTitleBar       │  │   │
│  │  ┌───────────┐  │     │  │  - 标题文本            │  │   │
│  │  │ TitleBar  │  │     │  │  - 关闭按钮            │  │   │
│  │  └───────────┘  │     │  │  - 拖拽支持            │  │   │
│  │                 │     │  └───────────────────────┘  │   │
│  │  WindowHelper   │     │  SubWindowHelper (C++)     │   │
│  │  (单例，主窗口)  │     │  (每个子窗口一个实例)       │   │
│  └─────────────────┘     └─────────────────────────────┘   │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 五、参考资料

- [Windows WM_NCHITTEST 消息](https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)
- [Qt QAbstractNativeEventFilter](https://doc.qt.io/qt-6/qabstractnativeeventfilter.html)
- [DWM Window Attributes](https://docs.microsoft.com/en-us/windows/win32/dwm/dwmwindowattributes)
- [Window Subclassing](https://docs.microsoft.com/en-us/windows/win32/controls/subclassing-overview)
