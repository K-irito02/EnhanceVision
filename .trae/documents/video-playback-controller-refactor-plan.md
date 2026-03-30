# 视频播放控制重构计划

## 需求分析

### 当前问题
`VideoPlaybackController.qml` 逻辑过于复杂，包含：
- 多个状态常量和状态机逻辑
- 重试机制 (`_waitForMediaReady`)
- 复杂的状态保存/恢复逻辑
- 多个标记变量 (`_isSourceResultSwitch`, `_videoEnded`)

### 目标功能
三个独立的控制按钮，互不影响：

| 按钮 | 控制场景 | 功能说明 |
|------|----------|----------|
| **开/切自动播放** | 打开/切换文件 | 从其他文件切换到视频，或初始点击视频时是否自动播放 |
| **源/结自动播放** | 源件/结果切换 | 点击"源件/结果"按钮切换时是否自动播放 |
| **源/结恢复进度** | 源件/结果切换 | 点击"源件/结果"按钮后播放进度是否保持一致 |

**关键约束**：用户的暂停/播放操作不影响以上三个按钮的功能。

---

## 重构方案

### 1. 简化 VideoPlaybackController.qml

#### 1.1 移除复杂状态机
删除以下内容：
- `stateIdle`, `stateLoaded`, `statePlaying` 等状态常量
- `_state` 属性及其相关逻辑
- `stateChanged` 信号
- `stateSavingState`, `stateLoadingSource`, `stateRestoringState` 状态

#### 1.2 移除重试机制
删除：
- `_waitForMediaReady()` 函数
- `retryTimer` 组件
- `_loadRetryCount` 属性

#### 1.3 保留核心属性
```qml
property var mediaPlayer: null
property bool isVideo: false
property string currentSource: ""
property var currentFile: null

property bool autoPlayOnOpen: SettingsController.videoAutoPlay
property bool autoPlayOnSwitch: SettingsController.videoAutoPlayOnSwitch
property bool restorePosition: SettingsController.videoRestorePosition

property var _savedState: null
```

### 2. 核心函数重构

#### 2.1 `handleFileOpen()` - 打开/切换文件
**触发时机**：用户从其他文件切换到视频，或初始打开视频

**逻辑**：
1. 清空 `_savedState`（新文件，无历史状态）
2. 等待媒体加载完成
3. 根据 `autoPlayOnOpen` 决定是否自动播放

```qml
function handleFileOpen() {
    _savedState = null  // 新文件，清空历史状态
    
    if (!mediaPlayer) return
    
    // 等待媒体加载完成后执行
    if (mediaPlayer.duration > 0) {
        if (autoPlayOnOpen) {
            mediaPlayer.play()
        }
    }
}
```

#### 2.2 `handleSourceResultSwitch()` - 源件/结果切换
**触发时机**：用户点击"源件/结果"按钮

**逻辑**：
1. 保存当前播放状态（位置、播放速率）
2. 切换源
3. 等待媒体加载完成后：
   - 根据 `restorePosition` 决定是否恢复进度
   - 根据 `autoPlayOnSwitch` 决定是否自动播放

```qml
function handleSourceResultSwitch(newSource) {
    if (!mediaPlayer) return
    
    // 保存当前状态
    _savedState = {
        position: mediaPlayer.position,
        playbackRate: mediaPlayer.playbackRate,
        wasPlaying: mediaPlayer.playbackState === MediaPlayer.PlayingState
    }
    
    // 切换源
    mediaPlayer.source = newSource
}

function handleMediaLoaded() {
    if (!_savedState) {
        // 非源件/结果切换，不处理
        return
    }
    
    // 恢复播放速率
    mediaPlayer.playbackRate = _savedState.playbackRate || 1.0
    
    // 恢复进度
    if (restorePosition && _savedState.position > 0) {
        mediaPlayer.position = Math.min(_savedState.position, mediaPlayer.duration)
    }
    
    // 自动播放
    if (autoPlayOnSwitch) {
        mediaPlayer.play()
    } else {
        mediaPlayer.pause()
    }
    
    _savedState = null
}
```

#### 2.3 移除 `handlePlaybackStateChanged()`
用户的暂停/播放操作不需要特殊处理，直接删除此函数。

### 3. 调用方修改

#### 3.1 EmbeddedMediaViewer.qml
修改 `onCurrentSourceChanged` 处理：
```qml
function onCurrentSourceChanged() {
    if (root.isVideo && root.currentSource) {
        var src = root._getSource(root.currentSource)
        vidPlayer.source = src
        playbackController.handleFileOpen()
    }
}
```

修改 `onShowOriginalChanged` 处理：
```qml
onShowOriginalChanged: {
    if (isVideo && _mediaPlayer) {
        playbackController.handleSourceResultSwitch(_getSource(currentSource))
    }
}
```

#### 3.2 MediaViewerWindow.qml
同上修改。

### 4. 删除的代码

| 文件 | 删除内容 |
|------|----------|
| VideoPlaybackController.qml | 状态常量、`_state`、`stateChanged`信号、`_waitForMediaReady()`、`retryTimer`、`progressSaveTimer`、`handlePlaybackStateChanged()`、`_videoEnded`、`_isSourceResultSwitch`、`errorOccurred`信号 |
| EmbeddedMediaViewer.qml | `_videoEnded` 属性、`onPlaybackStateChanged` 中的 `_videoEnded` 逻辑 |
| MediaViewerWindow.qml | 同上 |

---

## 实施步骤

1. **重构 VideoPlaybackController.qml**
   - 删除复杂状态机
   - 简化核心函数
   - 保留必要的保存/恢复逻辑

2. **修改 EmbeddedMediaViewer.qml**
   - 移除 `_videoEnded` 相关逻辑
   - 简化 `onPlaybackStateChanged` 处理

3. **修改 MediaViewerWindow.qml**
   - 同上修改

4. **构建验证**
   - 使用 `qt-build-and-fix` 技能构建项目
   - 测试三个按钮功能是否独立工作

---

## 功能验证清单

| 测试场景 | 预期行为 |
|----------|----------|
| 开/切自动播放=ON，打开视频 | 自动播放 |
| 开/切自动播放=OFF，打开视频 | 暂停状态 |
| 源/结自动播放=ON，切换源件/结果 | 自动播放 |
| 源/结自动播放=OFF，切换源件/结果 | 暂停状态 |
| 源/结恢复进度=ON，切换源件/结果 | 进度保持 |
| 源/结恢复进度=OFF，切换源件/结果 | 从头开始 |
| 用户暂停视频后切换文件 | 不影响开/切自动播放设置 |
| 用户暂停视频后切换源件/结果 | 不影响源/结自动播放设置 |
