# UI 优化与窗口修复笔记

## 概述

本次更新对项目进行了多项 UI 优化和 Bug 修复，包括缩略图渲染优化、抗锯齿处理、鼠标悬停交互优化、独立原生窗口 Bug 修复以及窗口拉伸功能完善。

**创建日期**: 2026-03-17
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 缩略图优化（三个位置）

| 组件 | 文件路径 | 优化内容 |
|------|---------|---------|
| MediaThumbnailStrip | `qml/components/MediaThumbnailStrip.qml` | 添加 `smooth: true`、`layer.samples: 4` 抗锯齿、悬停蓝色边框动画 |
| FileList | `qml/components/FileList.qml` | 添加 `smooth: true` 图像平滑处理 |
| MediaViewerWindow | `qml/components/MediaViewerWindow.qml` | 添加 `smooth: true`、`mipmap: true` 图像质量优化 |

### 2. 抗锯齿处理

| 技术 | 实现方式 | 应用位置 |
|------|---------|---------|
| 图像平滑 | `smooth: true` | 所有 Image 组件 |
| 几何抗锯齿 | `antialiasing: true` | Rectangle 圆角元素 |
| 多重采样 | `layer.samples: 4` | 缩略图遮罩层 |
| 多级纹理 | `mipmap: true` | 大图预览 |

### 3. 鼠标悬停蓝色边框优化

| 组件 | 文件路径 | 优化内容 |
|------|---------|---------|
| MediaThumbnailStrip | `qml/components/MediaThumbnailStrip.qml` | 添加独立 hoverBorder 层，平滑颜色过渡动画 |
| FileList | `qml/components/FileList.qml` | 卡片悬停时显示蓝色边框，使用 Behavior 动画 |

### 4. 独立原生窗口 Bug 修复

| 问题 | 文件路径 | 修复内容 |
|------|---------|---------|
| 关闭按钮不工作 | `qml/components/ShaderParamsPanel.qml` | 修正 windowRef 引用，使用 close() 替代 hide() |
| 窗口引用错误 | `qml/components/DialogTitleBar.qml` | 确保 windowRef 指向正确的 Window 对象 |

### 5. 窗口拉伸优化

| 功能 | 文件路径 | 优化内容 |
|------|---------|---------|
| 上边框拉伸 | `src/utils/SubWindowHelper.cpp` | 添加 HTTOP、HTTOPLEFT、HTTOPRIGHT 支持 |
| 四角拉伸 | `src/utils/SubWindowHelper.cpp` | 完善四角拉伸检测逻辑 |

---

## 二、技术实现细节

### 2.1 缩略图抗锯齿实现

```qml
// MediaThumbnailStrip.qml
Image {
    id: thumbImage
    anchors.fill: parent
    source: thumbnailPath
    fillMode: Image.PreserveAspectCrop
    asynchronous: true
    smooth: true                                    // 图像平滑
    sourceSize: Qt.size(root.thumbSize * 2, root.thumbSize * 2)  // 高分辨率源
    visible: status === Image.Ready
    layer.enabled: true
    layer.samples: 4                                // 多重采样抗锯齿
    layer.effect: MultiEffect {
        maskEnabled: true
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
        maskSource: thumbMask
    }
}

// 圆角遮罩
Item {
    id: thumbMask
    visible: false
    layer.enabled: true
    layer.samples: 4                                // 遮罩也启用抗锯齿
    width: thumbRect.width
    height: thumbRect.height

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.md
        color: "white"
        antialiasing: true                          // 几何抗锯齿
    }
}
```

### 2.2 悬停蓝色边框动画

```qml
// MediaThumbnailStrip.qml
Rectangle {
    id: hoverBorder
    anchors.fill: parent
    anchors.margins: 2
    radius: Theme.radius.md
    color: "transparent"
    border.width: 2
    border.color: thumbMouse.containsMouse ? Theme.colors.primary : "transparent"
    z: 5
    
    // 平滑颜色过渡动画
    Behavior on border.color { 
        ColorAnimation { duration: Theme.animation.fast } 
    }
}
```

### 2.3 窗口拉伸逻辑

```cpp
// SubWindowHelper.cpp - WM_NCHITTEST 消息处理
case WM_NCHITTEST: {
    // ... 获取坐标信息 ...
    
    int titleBarHeight = 40;
    int margin = helper->windowResizeMargin();

    // 标题栏区域处理
    if (localY >= 0 && localY < titleBarHeight) {
        bool inRightArea = localX >= windowWidth - 50;
        if (!inRightArea) {
            if (!helper->isWindowMaximized()) {
                bool onLeft = localX < margin;
                bool onRight = localX >= windowWidth - margin;
                bool onTop = localY < margin;
                
                // 四角和边框检测
                if (onTop && onLeft) return HTTOPLEFT;
                if (onTop && onRight) return HTTOPRIGHT;
                if (onTop) return HTTOP;           // 上边框拉伸
                if (onLeft) return HTLEFT;
                if (onRight) return HTRIGHT;
            }
            return HTCAPTION;
        }
    }
    
    // 客户区边框处理
    if (!helper->isWindowMaximized()) {
        bool onLeft = localX < margin;
        bool onRight = localX >= windowWidth - margin;
        bool onTop = localY < margin;
        bool onBottom = localY >= windowHeight - margin;

        if (onTop && onLeft) return HTTOPLEFT;
        if (onTop && onRight) return HTTOPRIGHT;
        if (onBottom && onLeft) return HTBOTTOMLEFT;
        if (onBottom && onRight) return HTBOTTOMRIGHT;
        if (onLeft) return HTLEFT;
        if (onRight) return HTRIGHT;
        if (onTop) return HTTOP;                   // 上边框拉伸
        if (onBottom) return HTBOTTOM;
    }
    break;
}
```

### 2.4 独立窗口关闭按钮修复

```qml
// ShaderParamsPanel.qml - 修正前
WindowTitleBar {
    windowRef: addCategoryHelper  // 错误：指向 SubWindowHelper
    onCloseClicked: addCategoryDialog.hide()  // 错误：使用 hide()
}

// ShaderParamsPanel.qml - 修正后
WindowTitleBar {
    windowRef: addCategoryDialog  // 正确：指向 Window 对象
    onCloseClicked: addCategoryDialog.close()  // 正确：使用 close()
}
```

---

## 三、修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/utils/SubWindowHelper.cpp` | 添加上边框拉伸支持（HTTOP、HTTOPLEFT、HTTOPRIGHT） |
| `qml/components/MediaThumbnailStrip.qml` | 添加 smooth、layer.samples 抗锯齿，添加悬停蓝色边框动画 |
| `qml/components/FileList.qml` | 添加 smooth 图像平滑处理，优化悬停边框动画 |
| `qml/components/MediaViewerWindow.qml` | 添加 smooth、mipmap 图像质量优化 |
| `qml/components/ShaderParamsPanel.qml` | 修正 windowRef 引用，使用 close() 替代 hide() |
| `qml/components/DialogTitleBar.qml` | 优化关闭按钮调用逻辑 |

---

## 四、遇到的问题及解决方案

### 问题 1：缩略图边缘锯齿严重

**现象**：缩略图圆角边缘有明显锯齿，视觉效果不佳。

**原因**：
1. Image 组件未启用 smooth 属性
2. 圆角遮罩未启用抗锯齿
3. 缩略图源尺寸过小

**解决方案**：
1. 启用 `smooth: true` 图像平滑
2. 启用 `layer.samples: 4` 多重采样抗锯齿
3. 设置 `sourceSize` 为显示尺寸的 2 倍
4. 遮罩 Rectangle 启用 `antialiasing: true`

### 问题 2：悬停边框闪烁

**现象**：鼠标悬停时边框颜色变化突兀，没有平滑过渡。

**原因**：未添加 Behavior 动画。

**解决方案**：
```qml
Behavior on border.color { 
    ColorAnimation { duration: Theme.animation.fast } 
}
```

### 问题 3：窗口上边框无法拉伸

**现象**：窗口左、右、下边框可以拉伸，但上边框和四角无法拉伸。

**原因**：SubWindowHelper 的 WM_NCHITTEST 处理中缺少 HTTOP 相关返回值。

**解决方案**：在标题栏区域和客户区边框检测中添加 HTTOP、HTTOPLEFT、HTTOPRIGHT 返回值。

### 问题 4：子窗口关闭按钮无效

**现象**：点击子窗口标题栏的 "x" 按钮无法关闭窗口。

**原因**：
1. windowRef 属性指向了 SubWindowHelper 而非 Window 对象
2. 使用 hide() 方法隐藏窗口而非 close() 关闭窗口

**解决方案**：
1. 修正 windowRef 指向正确的 Window 对象
2. 使用 close() 方法正确关闭窗口

---

## 五、性能影响

| 优化项 | 性能影响 | 说明 |
|--------|---------|------|
| smooth: true | 极小 | GPU 硬件加速，几乎无性能损耗 |
| layer.samples: 4 | 小 | 仅影响缩略图区域，GPU 多重采样 |
| mipmap: true | 小 | 仅大图预览使用，提升缩放质量 |
| Behavior 动画 | 极小 | 简单颜色动画，GPU 加速 |

---

## 六、视觉效果对比

### 缩略图渲染
- **优化前**：圆角边缘锯齿明显，图像模糊
- **优化后**：圆角边缘平滑，图像清晰锐利

### 悬停交互
- **优化前**：边框颜色突变，视觉跳跃
- **优化后**：边框颜色平滑过渡，交互自然

### 窗口拉伸
- **优化前**：仅支持左、右、下三边拉伸
- **优化后**：支持四边和四角全方位拉伸，符合 Windows 原生体验

---

## 七、相关文件

- [Shader 模式对话框关闭按钮修复笔记](./shader-dialog-close-button-fix.md)
- [自定义标题栏实现笔记](./custom-titlebar-implementation-notes.md)
- [自定义标题栏拖拽问题](./custom-titlebar-drag-issue.md)
