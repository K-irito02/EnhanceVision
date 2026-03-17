# 媒体查看器主题增强计划

## 问题概述

### 问题 1：视频预览帧缺失
- **现象**：视频在放大查看界面显示灰色背景，没有显示视频帧作为预览横幅
- **原因**：`MediaViewerWindow.qml` 中视频容器在未播放时只显示播放按钮，没有预览帧图像

### 问题 2：亮色主题下功能区域背景色问题
- **现象**：视频控制区域和底部缩略图栏在亮色主题下仍显示暗色背景
- **原因**：`videoControls` 和 `bottomBar` 使用硬编码的 `Qt.rgba(0, 0, 0, 0.6)` 等暗色

### 问题 3：图标颜色主题适配问题
- **现象**：主题切换后，视频控制区域的图标可能不显示
- **原因**：图标颜色硬编码为 `#FFFFFF`，未随主题变化

---

## 解决方案

### 方案 1：视频预览帧显示

#### 1.1 技术实现思路
利用现有的 `ThumbnailProvider` 和 `ImageUtils::generateVideoThumbnail` 功能，在视频加载时自动提取预览帧。

#### 1.2 实现步骤

**步骤 1：在 `MediaViewerWindow.qml` 中添加预览帧组件**
- 在 `videoContainer` 内添加一个 `Image` 组件用于显示视频预览帧
- 预览帧在视频未播放时显示，播放时隐藏
- 预览帧来源：`image://thumbnail/视频路径`

**步骤 2：优化预览帧加载逻辑**
- 当切换到视频文件时，自动加载预览帧
- 预览帧加载完成后显示，避免灰色背景
- 点击播放按钮后隐藏预览帧，显示视频画面

**步骤 3：添加加载状态指示**
- 预览帧加载中显示加载动画
- 加载失败时显示视频图标占位

---

### 方案 2：功能区域主题适配

#### 2.1 颜色系统扩展

**在 `Colors.qml` 中添加媒体查看器专用颜色：**

```qml
// 亮色主题
readonly property color mediaControlBg: Qt.rgba(255, 255, 255, 0.92)    // 半透明白色
readonly property color mediaControlBgLight: Qt.rgba(245, 248, 252, 0.95)
readonly property color mediaControlBorder: "#D0DAE8"
readonly property color mediaControlIcon: "#5A6A85"
readonly property color mediaControlIconHover: "#002FA7"
readonly property color mediaControlText: "#0A1628"
readonly property color mediaControlTextMuted: "#5A6A85"

// 暗色主题
readonly property color mediaControlBg: Qt.rgba(10, 20, 40, 0.88)       // 半透明深蓝
readonly property color mediaControlBgLight: Qt.rgba(15, 32, 64, 0.9)
readonly property color mediaControlBorder: "#152040"
readonly property color mediaControlIcon: "#A0C4FF"
readonly property color mediaControlIconHover: "#FFFFFF"
readonly property color mediaControlText: "#E8EDF5"
readonly property color mediaControlTextMuted: "#7A8BA8"
```

#### 2.2 MediaViewerWindow.qml 修改

**修改点：**

1. **窗口背景色**（第 58 行）
   - 当前：`color: "#0A0E1A"`（硬编码暗色）
   - 修改为：`color: Theme.colors.background`

2. **主容器背景色**（第 156 行）
   - 当前：`color: "#0A0E1A"`
   - 修改为：`color: Theme.colors.background`

3. **视频控制区域**（第 449 行）
   - 当前：`color: Qt.rgba(0, 0, 0, 0.6)`
   - 修改为：`color: Theme.colors.mediaControlBg`
   - 添加顶部边框：`border.color: Theme.colors.mediaControlBorder`

4. **底部缩略图栏**（第 671 行）
   - 当前：`color: Qt.rgba(0, 0, 0, 0.5)`
   - 修改为：`color: Theme.colors.mediaControlBgLight`

5. **左右导航按钮背景**（第 393、420 行）
   - 当前：`Qt.rgba(1,1,1,0.08)` 和 `Qt.rgba(1,1,1,0.2)`
   - 修改为：根据主题动态调整

---

### 方案 3：图标颜色主题适配

#### 3.1 需要修改的图标组件

| 组件位置 | 当前颜色 | 修改方案 |
|---------|---------|---------|
| 视频中央播放按钮 | `#FFFFFF` | `Theme.colors.mediaControlIcon` |
| 快退/快进按钮 | `#FFFFFF` | `Theme.colors.mediaControlIcon` |
| 播放/暂停按钮 | `#FFFFFF` | `Theme.colors.mediaControlIcon` |
| 音量按钮 | `#FFFFFF` | `Theme.colors.mediaControlIcon` |
| 左右导航按钮 | `#FFFFFF` | `Theme.colors.mediaControlIcon` |
| 底部缩略图视频图标 | `Qt.rgba(1, 1, 1, 0.3)` | 根据主题调整 |
| 底部缩略图播放小图标 | `#FFFFFF` | 保持白色（有深色背景） |

#### 3.2 IconButton 使用方式调整

当前 `IconButton` 已支持 `iconColor` 和 `iconHoverColor` 属性，只需传入正确的主题颜色即可。

---

## 实施任务清单

### 任务 1：扩展颜色系统
- [ ] 在 `Colors.qml` 亮色主题中添加媒体控制区域颜色
- [ ] 在 `Colors.qml` 暗色主题中添加媒体控制区域颜色

### 任务 2：实现视频预览帧功能
- [ ] 在 `MediaViewerWindow.qml` 的 `videoContainer` 中添加预览帧 Image 组件
- [ ] 实现预览帧的显示/隐藏逻辑（未播放时显示，播放时隐藏）
- [ ] 添加预览帧加载状态处理

### 任务 3：适配主题颜色
- [ ] 修改窗口和主容器背景色使用主题颜色
- [ ] 修改视频控制区域背景色和边框
- [ ] 修改底部缩略图栏背景色
- [ ] 修改左右导航按钮背景色

### 任务 4：适配图标颜色
- [ ] 修改视频控制区域所有图标颜色使用主题颜色
- [ ] 修改导航按钮图标颜色
- [ ] 修改底部缩略图相关图标颜色

### 任务 5：测试验证
- [ ] 测试暗色主题下视频预览帧显示
- [ ] 测试亮色主题下视频预览帧显示
- [ ] 测试主题切换时背景色和图标颜色变化
- [ ] 测试视频播放/暂停时预览帧的显示/隐藏

---

## 技术细节

### 视频预览帧实现代码示例

```qml
// 在 videoContainer 内添加预览帧
Image {
    id: videoPreviewFrame
    anchors.fill: parent
    fillMode: Image.PreserveAspectFit
    asynchronous: true
    smooth: true
    visible: isVideo && (!mediaPlayer || mediaPlayer.playbackState !== MediaPlayer.PlayingState)
    opacity: visible ? 1.0 : 0.0
    
    source: {
        if (!isVideo || !currentSource || currentSource === "") return ""
        return "image://thumbnail/" + currentSource
    }
    
    // 加载中占位
    Rectangle {
        anchors.fill: parent
        color: Theme.colors.card
        visible: parent.status === Image.Loading
        
        BusyIndicator {
            anchors.centerIn: parent
            running: parent.visible
        }
    }
    
    // 加载失败占位
    ColoredIcon {
        anchors.centerIn: parent
        source: Theme.icon("video")
        iconSize: 64
        color: Theme.colors.mutedForeground
        visible: parent.status === Image.Error
    }
    
    Behavior on opacity { NumberAnimation { duration: 200 } }
}
```

### 主题颜色使用示例

```qml
// 视频控制区域
Rectangle {
    id: videoControls
    color: Theme.colors.mediaControlBg
    
    // 顶部边框分隔线
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.colors.mediaControlBorder
    }
    
    // 图标按钮
    IconButton {
        iconName: "skip-back"
        iconColor: Theme.colors.mediaControlIcon
        iconHoverColor: Theme.colors.mediaControlIconHover
    }
}
```

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|-----|------|---------|
| 预览帧加载延迟 | 用户体验短暂灰色 | 添加加载动画，优化缩略图缓存 |
| 主题切换闪烁 | 视觉不连贯 | 使用 Behavior 动画平滑过渡 |
| 亮色主题对比度不足 | 图标不清晰 | 调整颜色确保足够对比度 |

---

## 预期效果

1. **视频预览帧**：打开视频时自动显示视频帧作为预览，不再显示灰色背景
2. **主题一致性**：亮色主题下，控制区域背景变为浅色，图标颜色自动适配
3. **平滑过渡**：主题切换时，所有颜色平滑过渡，无闪烁
