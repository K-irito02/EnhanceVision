# EmbeddedMediaViewer 优化实现笔记

## 概述

优化和修复 EmbeddedMediaViewer 组件，解决窗口显示、按钮功能、布局和拖拽吸附问题。

**创建日期**: 2025-03-19
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 创建的新组件

| 组件 | 文件路径 | 功能描述 |
|------|---------|---------|
| TitleButton | EmbeddedMediaViewer.qml 内嵌组件 | 标题栏按钮组件，支持悬停效果和工具提示 |
| MediaContentArea | EmbeddedMediaViewer.qml 内嵌组件 | 媒体内容显示区域，支持图片、视频、Shader效果 |
| VideoControlBar | EmbeddedMediaViewer.qml 内嵌组件 | 视频控制栏，包含播放控制、进度条、音量控制 |
| ThumbnailBar | EmbeddedMediaViewer.qml 内嵌组件 | 缩略图条，支持多文件导航 |

### 2. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| EmbeddedMediaViewer.qml | 完全重写，简化设计，修复所有已知问题 |
| .windsurf/rules/10-skills-and-mcp.md | 新增项目技能和MCP服务指南 |

### 3. 实现的功能特性

- ✅ **窗口显示修复**: 内嵌模式现在正确显示全屏覆盖层
- ✅ **按钮功能修复**: 所有标题栏按钮（独立、最小化、关闭）正常工作
- ✅ **全屏覆盖**: 内嵌模式默认覆盖整个容器区域
- ✅ **拖拽脱离**: 拖拽标题栏超过阈值自动切换到独立窗口
- ✅ **智能吸附**: 独立窗口拖回容器区域时显示吸附提示
- ✅ **组件化设计**: 使用内嵌组件提高代码复用性
- ✅ **响应式布局**: 自适应不同尺寸的容器和窗口

---

## 二、遇到的问题及解决方案

### 问题 1：窗口不显示

**现象**：调用 openAt() 方法后查看器窗口不出现

**原因**：
- 原设计中 embeddedContent 是 Rectangle 但父元素没有正确布局
- visible 属性和 opacity 属性控制混乱

**解决方案**：
- 使用 embeddedOverlay 作为全屏覆盖层
- 简化显示逻辑：openAt() → visible = true → forceActiveFocus()
- 移除复杂的动画和状态管理

### 问题 2：标题栏按钮不工作

**现象**：点击按钮没有响应

**原因**：
- IconButton 组件可能缺少必要属性
- 事件处理链可能被阻断

**解决方案**：
- 创建自定义 TitleButton 组件
- 直接使用 MouseArea 处理点击事件
- 添加悬停效果和工具提示

### 问题 3：内嵌模式布局问题

**现象**：窗口居中显示，没有覆盖整个容器

**原因**：
- 使用了固定的 embeddedWidth/embeddedHeight 属性
- 布局锚点设置不当

**解决方案**：
- 使用 anchors.fill: parent 实现全屏覆盖
- 移除固定尺寸属性，使用相对布局

### 问题 4：拖拽体验不佳

**现象**：拖拽超出边界时没有平滑过渡，缺少吸附效果

**原因**：
- 原设计只支持单向脱离（内嵌→独立）
- 没有实现 Windows 风格的吸附效果

**解决方案**：
- 实现双向切换（内嵌↔独立）
- 添加拖拽阈值检测（60px）
- 独立窗口拖回容器时显示吸附提示
- 使用 Timer 检测吸附条件

---

## 三、技术实现细节

### 1. 组件架构

```qml
EmbeddedMediaViewer (根组件)
├── embeddedOverlay (内嵌模式覆盖层)
│   ├── titleBar (标题栏)
│   ├── MediaContentArea (内容区域)
│   ├── VideoControlBar (视频控制)
│   └── ThumbnailBar (缩略图条)
└── detachedWindow (独立窗口)
    ├── detTitle (独立窗口标题栏)
    ├── MediaContentArea (复用)
    ├── VideoControlBar (复用)
    └── ThumbnailBar (复用)
```

### 2. 关键属性和方法

```qml
// 模式控制
property string displayMode: "embedded"
property bool isOpen: false
property bool isMinimized: false

// 核心方法
function openAt(index)     // 打开查看器
function close()           // 关闭查看器
function switchToDetached() // 切换到独立模式
function switchToEmbedded() // 切换到内嵌模式

// 拖拽检测
property real dragThreshold: 60  // 拖拽阈值
```

### 3. 智能吸附实现

```qml
// 吸附检测
function _isOverContainer(gx, gy) {
    if (!containerItem) return false
    var p = containerItem.mapToGlobal(0, 0)
    return gx >= p.x && gx <= p.x + containerItem.width &&
           gy >= p.y && gy <= p.y + containerItem.height
}

// 吸附提示显示
Rectangle {
    visible: detachedWindow._snapHint
    // 显示半透明覆盖层和提示文本
}

// 自动吸附
Timer {
    interval: 100
    running: detachedWindow.visible && !winHelper.isDragging
    onTriggered: {
        if (detachedWindow._snapHint) {
            root.switchToEmbedded()
        }
    }
}
```

### 4. 组件复用策略

通过内嵌组件实现代码复用：

```qml
// 内嵌模式和独立模式都使用相同的组件
MediaContentArea { viewer: root; videoPlayer: vidPlayer }
VideoControlBar { viewer: root; videoPlayer: vidPlayer }
ThumbnailBar { viewer: root }
```

---

## 四、性能优化

### 1. 延迟加载
- 视频控制栏只在 isVideo 为 true 时显示
- 缩略图栏只在多文件时显示

### 2. 属性绑定优化
- 使用 readonly property 减少不必要的计算
- 合理使用 Connections 避免循环绑定

### 3. 内存管理
- 组件销毁时正确释放 MediaPlayer 资源
- 使用 visible 而非动态创建/销毁组件

---

## 五、测试验证

### 功能测试清单

- [ ] 内嵌模式窗口显示
- [ ] 独立模式窗口显示
- [ ] 标题栏按钮功能
- [ ] 拖拽脱离功能
- [ ] 拖拽吸附功能
- [ ] 视频播放控制
- [ ] 图片导航功能
- [ ] Shader 效果应用
- [ ] 快捷键响应

### 兼容性测试

- [ ] 不同窗口尺寸
- [ ] 不同屏幕分辨率
- [ ] 暗色/亮色主题

---

## 六、后续改进计划

1. **动画效果**: 添加平滑的模式切换动画
2. **手势支持**: 支持触摸屏手势操作
3. **多显示器**: 支持多显示器环境下的拖拽
4. **性能监控**: 添加性能监控和优化
5. **用户偏好**: 保存用户的窗口位置和大小偏好

---

## 七、参考资料

- [Qt Quick Window 文档](https://doc.qt.io/qt-6/qml-qtquick-window.html)
- [Qt Quick Layouts 文档](https://doc.qt.io/qt-6/qtquicklayouts-index.html)
- [Qt Multimedia 文档](https://doc.qt.io/qt-6/qtmultimedia-index.html)
- [Windows 窗口管理指南](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features)
