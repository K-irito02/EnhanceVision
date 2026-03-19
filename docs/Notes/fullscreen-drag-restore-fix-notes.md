# 全屏窗口拖拽恢复功能修复笔记

## 概述

修复了独立窗口查看器在全屏状态下拖拽标题栏无法恢复窗口大小的问题，以及窗口层级管理、画面居中、导航按钮消失等相关问题。

**创建日期**: 2026-03-19
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/utils/SubWindowHelper.cpp` | 添加 `handleFullScreenDrag()` 和 `isWindowFullScreen()` 方法，在 `WM_NCLBUTTONDOWN` 消息中处理全屏拖拽 |
| `include/EnhanceVision/utils/SubWindowHelper.h` | 添加新方法声明 |
| `qml/components/EmbeddedMediaViewer.qml` | 修复窗口层级管理、画面居中、导航按钮消失问题，删除无效的 MouseArea |
| `qml/components/MediaViewerWindow.qml` | 修复导航按钮消失问题 |

### 2. 实现的功能特性

- ✅ 全屏状态下拖拽标题栏自动退出全屏并恢复窗口大小
- ✅ 窗口层级管理：后打开的查看器显示在上层
- ✅ 独立窗口画面居中显示
- ✅ 导航按钮悬停时不消失

---

## 二、遇到的问题及解决方案

### 问题 1：全屏状态下拖拽标题栏窗口不变小

**现象**：当查看器变成独立窗口时，点击全屏按钮后拖拽标题栏，窗口虽然能被拖动，但还是全屏的大小。

**原因**：
1. QML 的 `MouseArea` 在系统处理 `WM_NCHITTEST` 返回 `HTCAPTION` 后不会收到 `onReleased` 事件
2. 系统接管了鼠标事件，QML 层无法干预

**解决方案**：
在 `SubWindowWndProc` 中处理 `WM_NCLBUTTONDOWN` 消息，当窗口处于全屏状态时：
1. 调用 `handleFullScreenDrag()` 方法
2. 退出全屏 (`showNormal()`)
3. 恢复之前保存的窗口几何信息
4. 发送 `WM_NCLBUTTONDOWN` 消息启动系统拖拽

### 问题 2：窗口层级管理问题

**现象**：当两个查看器标签都是最小化时，打开待处理查看器，再打开消息展示查看器时，消息展示查看器并没有出现在上层。

**原因**：`embeddedOverlay` 的 `z` 属性硬编码为 1000，没有跟随 `root.z` 动态变化。

**解决方案**：将 `z: 1000` 改为 `z: root.z`，使层级跟随全局计数器动态调整。

### 问题 3：独立窗口画面不居中

**现象**：当查看器变成独立窗口时，窗口显示的多媒体画面没有在显示区域上下居中。

**原因**：`MediaContentArea` 设置了 `anchors.margins: 16`，但内部的 Image 使用 `PreserveAspectFit`，导致图像偏移。

**解决方案**：移除 `anchors.margins`，让内容区域填满整个空间，图像自动居中。

### 问题 4：导航按钮悬停时消失

**现象**：鼠标悬停在"左/右"两个多媒体文件切换按钮上时，这两个按钮消失了。

**原因**：父级 MouseArea 的 `onExited` 在鼠标进入按钮区域时触发，导致按钮立即隐藏。

**解决方案**：
1. 添加 `prevButtonHovered` 和 `nextButtonHovered` 属性跟踪按钮悬停状态
2. 当鼠标进入按钮时停止隐藏计时器并设置悬停状态
3. 当鼠标离开按钮时启动隐藏计时器
4. Timer 触发时检查是否有按钮处于悬停状态

---

## 三、技术实现细节

### 1. Windows 消息处理

```cpp
case WM_NCLBUTTONDOWN: {
    if (wParam == HTCAPTION && helper->isWindowFullScreen()) {
        RECT windowRect;
        GetWindowRect(hwnd, &windowRect);
        
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        
        int localX = x - windowRect.left;
        int localY = y - windowRect.top;
        int windowWidth = windowRect.right - windowRect.left;
        
        helper->handleFullScreenDrag(localX, localY, windowWidth);
        return 0;
    }
    break;
}
```

### 2. 全屏拖拽处理

```cpp
void SubWindowHelper::handleFullScreenDrag(int mouseX, int mouseY, int areaWidth)
{
    // 计算鼠标在标题栏中的相对位置比例
    qreal xRatio = static_cast<qreal>(mouseX) / areaWidth;
    
    // 根据比例计算恢复后窗口的位置
    int newWindowX = static_cast<int>(normalGeom.width() * xRatio);
    normalGeom.moveTopLeft(QPoint(fullScreenGeom.left() + mouseX - newWindowX, fullScreenGeom.top() + mouseY));
    
    // 退出全屏并设置窗口几何
    m_window->showNormal();
    m_window->setGeometry(normalGeom);
    
    // 启动系统拖拽
    ReleaseCapture();
    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
}
```

### 3. 窗口几何保存

在进入全屏前保存窗口几何信息：
```qml
IconButton { 
    onClicked: { 
        if (detachedWindow.visibility === Window.FullScreen) {
            // 退出全屏并恢复
        } else {
            // 保存窗口几何信息
            winHelper.saveNormalGeometry()
            detachedWindow._savedX = detachedWindow.x
            detachedWindow._savedY = detachedWindow.y
            detachedWindow._savedW = detachedWindow.width
            detachedWindow._savedH = detachedWindow.height
            detachedWindow.showFullScreen()
        }
    }
}
```

---

## 四、参考资料

- [Windows 消息处理](https://docs.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
- [WM_NCHITTEST 消息](https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)
- [Qt Window States](https://doc.qt.io/qt-6/qt.html#WindowState-enum)
