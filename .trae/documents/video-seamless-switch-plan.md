# 视频源件/结果无缝切换优化方案

## 问题分析

### 当前实现问题

在 `MediaViewerWindow.qml` 和 `EmbeddedMediaViewer.qml` 中，当用户点击"源件/结果"按钮切换视频时：

1. `showOriginal` 属性改变
2. `currentSource` 计算属性随之改变
3. `onCurrentSourceChanged` 信号处理器被触发
4. 视频被停止（`stop()`），然后加载新源
5. 如果 `autoPlayEnabled` 为 true，从头开始播放

**核心问题**：切换时视频进度丢失，播放状态未保持。

### 涉及文件

| 文件 | 职责 |
|------|------|
| `qml/components/MediaViewerWindow.qml` | 独立窗口媒体查看器 |
| `qml/components/EmbeddedMediaViewer.qml` | 嵌入式/独立窗口双模式查看器 |

---

## 解决方案

### 设计原则

1. **进度保持**：切换前保存播放进度，切换后恢复
2. **状态保持**：根据切换前的播放状态决定切换后行为
3. **无缝体验**：最小化视觉中断，避免闪烁
4. **线程安全**：使用 Qt.callLater 确保操作在正确线程执行
5. **内存安全**：避免循环引用，正确清理资源

### 核心实现思路

```
切换前：
  1. 保存当前播放进度 (position)
  2. 保存当前播放状态 (playing/paused/stopped)
  3. 保存播放速率 (playbackRate)

切换中：
  4. 加载新视频源
  5. 等待媒体状态变为 Loaded

切换后：
  6. 恢复播放进度
  7. 根据之前状态决定是否播放
  8. 恢复播放速率
```

---

## 详细实现步骤

### 步骤 1：添加播放状态保存属性

在两个查看器组件中添加：

```qml
// 播放状态保存（用于源件/结果切换时恢复）
property real _savedPosition: 0          // 保存的播放进度
property int _savedPlaybackState: 0      // 保存的播放状态 (0=stopped, 1=playing, 2=paused)
property real _savedPlaybackRate: 1.0    // 保存的播放速率
property bool _isSwitchingSource: false  // 是否正在进行源切换
```

### 步骤 2：创建状态保存函数

```qml
function _savePlaybackState() {
    if (!mediaPlayer) return
    _savedPosition = mediaPlayer.position
    _savedPlaybackRate = mediaPlayer.playbackRate
    
    if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
        _savedPlaybackState = 1  // playing
    } else if (mediaPlayer.playbackState === MediaPlayer.PausedState) {
        _savedPlaybackState = 2  // paused
    } else {
        _savedPlaybackState = 0  // stopped
    }
}
```

### 步骤 3：创建状态恢复函数

```qml
function _restorePlaybackState() {
    if (!mediaPlayer) return
    
    // 恢复播放进度
    if (_savedPosition > 0 && mediaPlayer.duration > 0) {
        mediaPlayer.position = Math.min(_savedPosition, mediaPlayer.duration)
    }
    
    // 恢复播放速率
    mediaPlayer.playbackRate = _savedPlaybackRate
    
    // 根据之前的状态决定是否播放
    if (_savedPlaybackState === 1) {
        // 之前是播放状态，继续播放
        mediaPlayer.play()
    } else if (_savedPlaybackState === 2) {
        // 之前是暂停状态，保持暂停
        mediaPlayer.pause()
    }
    // stopped 状态不需要额外操作
}
```

### 步骤 4：修改 showOriginal 切换逻辑

在 `onShowOriginalChanged` 信号处理器中：

```qml
onShowOriginalChanged: {
    if (!isVideo) return
    
    // 标记正在切换源
    _isSwitchingSource = true
    
    // 保存当前播放状态
    _savePlaybackState()
}
```

### 步骤 5：修改 onCurrentSourceChanged 逻辑

```qml
onCurrentSourceChanged: {
    if (isVideo && currentSource && currentSource !== "") {
        var src = currentSource
        if (!src.startsWith("file:///") && !src.startsWith("qrc:/")) {
            src = "file:///" + src
        }
        
        if (_isSwitchingSource) {
            // 源件/结果切换模式：保持进度
            mediaPlayer.source = src
            // 等待媒体加载完成后恢复状态
            Qt.callLater(function() {
                _restorePlaybackState()
                _isSwitchingSource = false
            })
        } else {
            // 正常切换文件模式：从头播放
            if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                mediaPlayer.stop()
            }
            mediaPlayer.source = src
            if (autoPlayEnabled && mediaPlayer.playbackState === MediaPlayer.StoppedState) {
                Qt.callLater(function() {
                    mediaPlayer.play()
                })
            }
        }
    } else {
        mediaPlayer.source = ""
    }
}
```

### 步骤 6：处理边界情况

#### 6.1 视频已播放完毕的情况

```qml
function _savePlaybackState() {
    if (!mediaPlayer) return
    
    // 如果视频已播放完毕，重置为从头开始
    if (_videoEnded) {
        _savedPosition = 0
        _savedPlaybackState = 0  // stopped
    } else {
        _savedPosition = mediaPlayer.position
        // ... 其他保存逻辑
    }
}
```

#### 6.2 新源加载失败的情况

```qml
MediaPlayer {
    // ...
    onErrorChanged: {
        if (error !== MediaPlayer.NoError) {
            console.warn("MediaPlayer error:", errorString)
            _isSwitchingSource = false
        }
    }
    
    onStatusChanged: {
        if (status === MediaPlayer.Loaded && _isSwitchingSource) {
            // 媒体加载完成，恢复播放状态
            Qt.callLater(function() {
                _restorePlaybackState()
                _isSwitchingSource = false
            })
        } else if (status === MediaPlayer.Invalid && _isSwitchingSource) {
            // 加载失败
            console.warn("Failed to load media:", source)
            _isSwitchingSource = false
        }
    }
}
```

#### 6.3 快速连续切换的防抖处理

```qml
Timer {
    id: sourceSwitchDebounce
    interval: 50
    repeat: false
    onTriggered: {
        // 延迟执行实际的源切换
        _performSourceSwitch()
    }
}

property string _pendingSource: ""

function _performSourceSwitch() {
    if (_pendingSource === "") return
    mediaPlayer.source = _pendingSource
    _pendingSource = ""
}
```

---

## 线程安全与内存管理

### 线程安全措施

1. **使用 Qt.callLater**：确保 UI 操作在主线程执行
2. **状态标志保护**：使用 `_isSwitchingSource` 防止并发操作
3. **信号处理顺序**：确保状态保存发生在源切换之前

### 内存管理措施

1. **避免循环引用**：不保存 mediaPlayer 的直接引用，使用属性访问
2. **及时清理状态**：切换完成后重置 `_isSwitchingSource` 标志
3. **资源释放**：在组件销毁时停止播放

```qml
Component.onDestruction: {
    if (mediaPlayer && mediaPlayer.playbackState !== MediaPlayer.StoppedState) {
        mediaPlayer.stop()
    }
}
```

---

## UI 流畅性优化

### 视觉连续性

1. **保持视频预览帧显示**：在切换过程中显示最后一帧
2. **避免黑屏闪烁**：使用 opacity 动画过渡
3. **进度条平滑过渡**：进度条值变化使用动画

```qml
Slider {
    // ...
    Behavior on value {
        enabled: !_isSwitchingSource
        NumberAnimation { duration: 100 }
    }
}
```

### 用户反馈

1. **切换时显示加载指示器**（可选，如果切换耗时较长）
2. **保持控制栏状态**：音量、播放速率等设置不变

---

## 实施清单

### MediaViewerWindow.qml 修改

- [ ] 添加播放状态保存属性
- [ ] 实现 `_savePlaybackState()` 函数
- [ ] 实现 `_restorePlaybackState()` 函数
- [ ] 添加 `onShowOriginalChanged` 处理器
- [ ] 修改 `onCurrentSourceChanged` 逻辑
- [ ] 添加 MediaPlayer 状态监听
- [ ] 添加错误处理
- [ ] 添加组件销毁时的清理

### EmbeddedMediaViewer.qml 修改

- [ ] 添加播放状态保存属性
- [ ] 实现 `_savePlaybackState()` 函数
- [ ] 实现 `_restorePlaybackState()` 函数
- [ ] 添加 `onShowOriginalChanged` 处理器
- [ ] 修改 `onCurrentSourceChanged` 逻辑
- [ ] 添加 MediaPlayer 状态监听
- [ ] 添加错误处理
- [ ] 添加组件销毁时的清理

---

## 测试场景

1. **基本切换**：播放中切换源件/结果，验证进度保持
2. **暂停状态切换**：暂停中切换，验证状态保持
3. **停止状态切换**：停止状态切换，验证不自动播放
4. **播放完毕切换**：视频播放完毕后切换，验证从头开始
5. **快速连续切换**：快速多次点击切换按钮，验证稳定性
6. **跨文件切换**：切换到不同文件，验证从头播放（非无缝）
7. **边界情况**：空源、无效源、网络源等

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 媒体加载延迟 | 用户体验下降 | 使用状态标志和异步回调 |
| 快速切换导致状态混乱 | 播放异常 | 添加防抖和状态检查 |
| 不同视频格式兼容性 | 进度恢复失败 | 添加错误处理和回退逻辑 |
| 内存泄漏 | 长期运行性能下降 | 正确清理资源和引用 |

---

## 预期效果

1. 用户点击"源件/结果"按钮时，视频无缝切换
2. 播放进度保持不变
3. 播放状态（播放/暂停）保持不变
4. 播放速率保持不变
5. UI 操作流畅，无卡顿或闪烁
6. 线程安全，无竞态条件
7. 内存使用稳定，无泄漏
