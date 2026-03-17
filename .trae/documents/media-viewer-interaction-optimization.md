# 多媒体查看器交互优化计划

## 问题分析

### 当前实现存在的问题

1. **前进/后退按钮始终可见**
   - 左右两侧的前进/后退按钮（第447-503行）始终显示
   - 没有自动隐藏/显示的交互效果
   - 可能遮挡媒体内容，影响观看体验

2. **媒体画面边距过大**
   - 视频和图片都有 `anchors.margins: 20` 的边距
   - 画面没有充分利用可用空间

3. **视频点击交互不完整**
   - 只有中间的播放按钮可以点击暂停/播放
   - 点击视频画面其他区域没有响应

## 优化方案

### 1. 前进/后退按钮自动隐藏效果

**设计方案：**
- 鼠标进入内容区域时，按钮淡入显示
- 鼠标离开内容区域后，延迟 2 秒自动淡出隐藏
- 鼠标悬停在按钮上时，保持显示状态
- 使用平滑的透明度动画过渡

**实现细节：**
```
状态机设计：
- idle: 按钮隐藏，等待鼠标进入
- visible: 按钮显示，鼠标在内容区域
- hovering: 鼠标悬停在按钮上，保持显示
- hiding: 延迟隐藏中，鼠标可能重新进入
```

### 2. 媒体画面铺满优化

**设计方案：**
- 减少或移除 `anchors.margins`
- 保持 `PreserveAspectFit` 防止画面变形
- 添加最小边距（如 8px）保持美观
- 可选：添加"适应/填充"模式切换

**实现细节：**
- 视频区域：`anchors.margins: 8`
- 图片区域：`anchors.margins: 8`
- 保持 `fillMode: Image.PreserveAspectFit`

### 3. 视频点击暂停/播放

**设计方案：**
- 在 `videoContainer` 上添加透明 `MouseArea`
- 单击切换播放/暂停状态
- 双击切换全屏模式
- 确保不影响底部控制栏的交互

**实现细节：**
- 使用 `onClicked` 处理单击事件
- 使用 `onDoubleClicked` 处理双击全屏
- 设置 `z` 值确保不影响其他控件

### 4. 图片点击交互增强

**设计方案：**
- 图片区域也支持前进/后退按钮自动隐藏
- 单击显示/隐藏控制元素
- 双击切换全屏模式

## 实现步骤

### Step 1: 添加鼠标悬停状态管理
- 在 `contentArea` 添加 `MouseArea` 检测鼠标位置
- 添加 `Timer` 实现延迟隐藏逻辑
- 添加 `hoverActive` 状态属性

### Step 2: 实现前进/后退按钮动画
- 为左右按钮添加 `opacity` 属性绑定
- 添加 `Behavior on opacity` 动画
- 绑定到 `hoverActive` 状态

### Step 3: 优化媒体画面边距
- 修改 `videoContainer` 的 `anchors.margins`
- 修改 `imageView` 的 `anchors.margins`
- 确保画面铺满效果

### Step 4: 添加视频点击交互
- 在 `videoContainer` 内添加点击区域
- 实现单击暂停/播放
- 实现双击全屏切换

### Step 5: 添加图片点击交互
- 在 `imageView` 区域添加点击区域
- 实现单击显示/隐藏控制元素
- 实现双击全屏切换

### Step 6: 测试验证
- 测试按钮自动隐藏/显示效果
- 测试视频点击暂停/播放
- 测试图片点击交互
- 测试全屏模式下的交互

## 修改文件

| 文件 | 修改内容 |
|------|----------|
| `qml/components/MediaViewerWindow.qml` | 主要修改文件，实现所有优化功能 |

## 代码修改要点

### 1. 添加状态属性和计时器

```qml
// 在 root 中添加
property bool hoverActive: false
property bool _buttonsHovered: false

Timer {
    id: hideButtonsTimer
    interval: 2000
    running: false
    repeat: false
    onTriggered: {
        if (!_buttonsHovered) {
            hoverActive = false
        }
    }
}
```

### 2. 内容区域鼠标检测

```qml
MouseArea {
    id: contentHoverArea
    anchors.fill: parent
    hoverEnabled: true
    propagateComposedEvents: true
    acceptedButtons: Qt.NoButton
    
    onContainsMouseChanged: {
        if (containsMouse) {
            hoverActive = true
            hideButtonsTimer.stop()
        } else if (!_buttonsHovered) {
            hideButtonsTimer.restart()
        }
    }
}
```

### 3. 按钮透明度动画

```qml
// 左侧后退按钮
Rectangle {
    id: prevButton
    // ... 现有属性
    opacity: hoverActive || prevMouse.containsMouse ? 1.0 : 0.0
    visible: currentIndex > 0
    
    Behavior on opacity { 
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }
}

// 右侧前进按钮
Rectangle {
    id: nextButton
    // ... 现有属性
    opacity: hoverActive || nextMouse.containsMouse ? 1.0 : 0.0
    visible: currentIndex < mediaFiles.length - 1
    
    Behavior on opacity { 
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }
}
```

### 4. 视频点击交互

```qml
MouseArea {
    id: videoClickArea
    anchors.fill: parent
    hoverEnabled: true
    propagateComposedEvents: true
    z: 5
    
    onClicked: function(mouse) {
        if (mediaPlayer) {
            mediaPlayer.togglePlay()
        }
    }
    
    onDoubleClicked: {
        _toggleFullscreen()
    }
}
```

### 5. 图片点击交互

```qml
MouseArea {
    id: imageClickArea
    anchors.fill: parent
    hoverEnabled: true
    propagateComposedEvents: true
    
    onClicked: {
        // 切换控制元素显示状态
        hoverActive = !hoverActive
    }
    
    onDoubleClicked: {
        _toggleFullscreen()
    }
}
```

## 预期效果

1. **按钮自动隐藏**：鼠标离开内容区域 2 秒后，前进/后退按钮自动淡出隐藏
2. **按钮自动显示**：鼠标进入内容区域时，按钮立即淡入显示
3. **视频铺满**：视频画面边距从 20px 减少到 8px，更充分利用空间
4. **图片铺满**：图片画面边距从 20px 减少到 8px
5. **视频点击**：单击视频画面切换播放/暂停，双击切换全屏
6. **图片点击**：单击图片切换控制元素显示，双击切换全屏
