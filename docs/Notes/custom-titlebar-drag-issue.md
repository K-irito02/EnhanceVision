# Qt 无边框窗口自定义标题栏拖拽问题解决方案

## 问题背景

在 Qt Quick (QML) + Qt Widgets (QQuickWidget) 混合架构中，实现无边框窗口的自定义标题栏时，遇到以下问题：

1. **最大化状态下无法拖拽**：窗口最大化后，标题栏拖拽不起作用
2. **最大化后拖拽无法连续操作**：从最大化状态拖拽恢复后，窗口无法再次拖拽
3. **Windows Snap 吸附后无法拖拽**：窗口被 Windows Snap 功能吸附到屏幕边缘后，无法再次拖拽
4. **标题栏按钮不起作用**：使用 `WM_NCHITTEST` 返回 `HTCAPTION` 后，标题栏上的按钮无法点击

## 根本原因

### Windows 消息处理机制

Windows 系统通过 `WM_NCHITTEST` 消息来确定鼠标位置对应的窗口区域：

- `HTCAPTION`：表示鼠标在标题栏区域，系统会自动处理拖拽
- `HTCLIENT`：表示鼠标在客户区，鼠标事件传递给应用程序
- `HTLEFT/HTRIGHT/HTTOP/HTBOTTOM`：表示鼠标在边框区域，系统会处理窗口调整大小

### 问题分析

1. **最大化状态下的拖拽**：当窗口最大化时，`WM_NCHITTEST` 被禁用或返回非 `HTCAPTION` 值，导致系统不处理拖拽

2. **拖拽后无法再次操作**：使用 `WM_SYSCOMMAND` 或 `startSystemMove()` 后，鼠标捕获状态可能未正确释放

3. **按钮无法点击**：当 `WM_NCHITTEST` 返回 `HTCAPTION` 时，系统会拦截所有鼠标事件，导致 Qt 无法接收到点击事件

## 解决方案

### 核心思路

**分离按钮区域和拖拽区域**：在 `WM_NCHITTEST` 处理中，检查鼠标是否在按钮区域：
- 如果在按钮区域，返回 `false` 让 Qt 处理鼠标事件
- 如果在空白区域，返回 `HTCAPTION` 让 Windows 系统处理拖拽

### 实现细节

#### 1. WindowHelper 类扩展

```cpp
// WindowHelper.h
class WindowHelper : public QObject, public QAbstractNativeEventFilter
{
    // ...
    Q_INVOKABLE void clearExcludeRegions();
    Q_INVOKABLE void addExcludeRegion(int x, int y, int width, int height);
    
private:
    QVector<QRect> m_excludeRegions;  // 排除区域列表
};
```

#### 2. WM_NCHITTEST 处理逻辑

```cpp
bool WindowHelper::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    if (msg->message == WM_NCHITTEST && m_window) {
        // 计算鼠标在窗口中的本地坐标
        int localX = x - windowRect.left;
        int localY = y - windowRect.top;
        
        // 标题栏区域 (Y < 48)
        if (localY >= 0 && localY < 48) {
            // 检查是否在排除区域（按钮区域）
            for (const QRect& region : m_excludeRegions) {
                if (region.contains(localX, localY)) {
                    return false;  // 让 Qt 处理鼠标事件
                }
            }
            *result = HTCAPTION;  // 让 Windows 处理拖拽
            return true;
        }
        
        // 边缘调整大小处理...
    }
}
```

#### 3. QML 中注册排除区域

```qml
// TitleBar.qml
Rectangle {
    id: root
    
    function updateExcludeRegions() {
        WindowHelper.clearExcludeRegions()
        
        // 左侧按钮组区域
        if (leftButtonsRow.width > 0) {
            WindowHelper.addExcludeRegion(leftButtonsRow.x, leftButtonsRow.y, 
                                          leftButtonsRow.width, leftButtonsRow.height)
        }
        
        // 右侧按钮组区域
        if (rightButtonsRow.width > 0) {
            WindowHelper.addExcludeRegion(rightButtonsRow.x, rightButtonsRow.y,
                                          rightButtonsRow.width, rightButtonsRow.height)
        }
    }
    
    Component.onCompleted: {
        updateExcludeRegions()
    }
    
    onWidthChanged: {
        updateExcludeRegions()
    }
}
```

### 关键技术点

1. **DWM 圆角和过渡禁用**：
   ```cpp
   DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
   DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
   
   BOOL disableTransitions = TRUE;
   DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disableTransitions, sizeof(disableTransitions));
   ```

2. **窗口最小尺寸限制**：
   ```cpp
   if (msg->message == WM_GETMINMAXINFO) {
       MINMAXINFO* minMaxInfo = reinterpret_cast<MINMAXINFO*>(msg->lParam);
       minMaxInfo->ptMinTrackSize.x = m_minWidth;
       minMaxInfo->ptMinTrackSize.y = m_minHeight;
   }
   ```

3. **边缘调整大小**：通过 `WM_NCHITTEST` 返回 `HTLEFT/HTRIGHT/HTTOP/HTBOTTOM` 等值实现

## 最终效果

- ✅ 标题栏按钮正常工作（新建会话、侧边栏切换、主题切换、最小化、最大化、关闭等）
- ✅ 标题栏空白区域支持拖拽
- ✅ 最大化状态下支持拖拽恢复
- ✅ Windows Snap 吸附后支持拖拽
- ✅ 窗口边缘支持调整大小
- ✅ 窗口有圆角效果
- ✅ 窗口有最小尺寸限制

## 文件修改清单

| 文件 | 修改内容 |
|------|---------|
| `include/EnhanceVision/utils/WindowHelper.h` | 添加 `clearExcludeRegions()` 和 `addExcludeRegion()` 方法 |
| `src/utils/WindowHelper.cpp` | 修改 `nativeEventFilter()` 处理逻辑，添加排除区域管理 |
| `qml/components/TitleBar.qml` | 添加 `updateExcludeRegions()` 函数，移除 MouseArea |

## 参考资料

- [Windows WM_NCHITTEST 消息](https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)
- [Qt QAbstractNativeEventFilter](https://doc.qt.io/qt-6/qabstractnativeeventfilter.html)
- [DWM Window Attributes](https://docs.microsoft.com/en-us/windows/win32/dwm/dwmwindowattributes)

---

**创建日期**: 2026-03-16  
**作者**: AI Assistant
