# 视频播放控制功能优化方案

## 一、代码审查发现的问题

### 1.1 核心问题：状态切换标志管理混乱

**问题位置**: [EmbeddedMediaViewer.qml:22-29](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/EmbeddedMediaViewer.qml#L22-L29)

```javascript
onShowOriginalChanged: {
    if (isVideo && _mediaPlayer) {
        _savePlaybackState()
        _isSwitchingSource = true
    }
}
```

**问题描述**:
- `_isSwitchingSource` 标志仅在 `isVideo && _mediaPlayer` 条件满足时设置
- 如果条件不满足，后续的 `onCurrentSourceChanged` 会走错误的分支
- 没有考虑 `_mediaPlayer` 为 null 的边界情况

### 1.2 状态保存时机问题

**问题位置**: [EmbeddedMediaViewer.qml:82-98](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/EmbeddedMediaViewer.qml#L82-L98)

**问题描述**:
- `_savePlaybackState()` 在 `onShowOriginalChanged` 中同步调用
- 此时 MediaPlayer 可能正在切换源，状态不稳定
- `_videoEnded` 状态检查可能导致进度丢失

### 1.3 状态恢复条件过于严格

**问题位置**: [EmbeddedMediaViewer.qml:100-148](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/EmbeddedMediaViewer.qml#L100-L148)

**问题描述**:
```javascript
if (!_mediaPlayer || _mediaPlayer.duration <= 0) {
    return  // 过早返回，导致恢复失败
}
```
- `duration <= 0` 检查在媒体刚加载完成时可能失败
- 没有重试机制处理暂时性失败

### 1.4 媒体加载事件处理问题

**问题位置**: [EmbeddedMediaViewer.qml:618-630](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/EmbeddedMediaViewer.qml#L618-L630)

**问题描述**:
- `Qt.callLater` 可能导致时序问题
- `_isSwitchingSource = false` 在 `Qt.callLater` 回调中设置，可能与其他事件冲突

### 1.5 状态冲突处理缺失

**问题描述**:
当前实现没有明确的状态优先级和冲突解决策略：
- "开/切自动播放" (videoAutoPlay) - 打开视频或切换文件时自动播放
- "源/结自动播放" (videoAutoPlayOnSwitch) - 源件/结果切换时自动播放
- "源/结恢复进度" (videoRestorePosition) - 恢复播放进度
- 用户手动操作 - 用户点击播放/暂停

### 1.6 进度持久化缺失

**问题描述**:
- 进度数据仅保存在内存中 (`_savedPosition`, `_savedPlaybackState`)
- 应用重启后无法恢复观看进度
- 没有跨会话的进度存储机制

### 1.7 线程安全问题

**问题描述**:
- QML 中的状态变量没有线程安全保护
- 多个信号处理器可能同时修改状态
- MediaPlayer 的状态变化可能在不同的线程触发

---

## 二、优化方案设计

### 2.1 方案概述

采用**状态机 + 事件驱动**架构，实现清晰的状态转换和冲突解决机制。

### 2.2 状态机设计

```
┌─────────────────────────────────────────────────────────────────┐
│                      VideoPlaybackStateMachine                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌─────────┐    loadMedia    ┌──────────┐    play    ┌───────┐ │
│  │  Idle   │ ──────────────→ │  Loaded  │ ─────────→ │Playing│ │
│  └─────────┘                 └──────────┘            └───────┘ │
│       ↑                           │                       │     │
│       │                           │ pause                 │     │
│       │                           ↓                       │     │
│       │                      ┌─────────┐                  │     │
│       └──────────────────────│ Paused  │←─────────────────┘     │
│           stop/reset         └─────────┘                        │
│                                                                   │
│  切换源件/结果时的状态转换:                                        │
│  Playing → SavingState → LoadingNewSource → RestoringState       │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 核心数据结构

```javascript
// 播放状态枚举
const PlaybackState = {
    Stopped: 0,
    Playing: 1,
    Paused: 2
}

// 切换上下文
QtObject {
    id: switchContext
    
    property bool isSwitching: false           // 是否正在切换
    property string switchType: ""             // "source_result" | "file_change"
    property var savedState: ({                // 保存的状态
        position: 0,
        playbackState: PlaybackState.Stopped,
        playbackRate: 1.0,
        timestamp: 0
    })
    property int retryCount: 0                  // 重试计数
    property int maxRetries: 3                  // 最大重试次数
}
```

### 2.4 状态优先级与冲突解决策略

```
优先级从高到低:
1. 用户手动操作 (最高优先级)
   - 用户点击播放/暂停时，忽略自动播放设置
   
2. 源件/结果切换 (videoAutoPlayOnSwitch, videoRestorePosition)
   - 切换时根据这两个设置决定行为
   
3. 文件切换 (videoAutoPlay)
   - 切换到新文件时根据此设置决定行为
   
4. 默认行为 (最低优先级)
   - 不自动播放，从头开始

冲突解决规则:
┌────────────────────────────────────────────────────────────────┐
│ 场景                    │ videoAutoPlay │ videoAutoPlayOnSwitch │ videoRestorePosition │ 行为
├────────────────────────────────────────────────────────────────┤
│ 打开新视频              │ ON            │ -                     │ -                    │ 自动播放
│ 打开新视频              │ OFF           │ -                     │ -                    │ 不播放
│ 切换文件                │ ON            │ -                     │ -                    │ 自动播放
│ 切换文件                │ OFF           │ -                     │ -                    │ 保持原状态
│ 源件↔结果切换           │ -             │ ON                    │ ON                   │ 恢复进度+自动播放
│ 源件↔结果切换           │ -             │ ON                    │ OFF                  │ 从头+自动播放
│ 源件↔结果切换           │ -             │ OFF                   │ ON                   │ 恢复进度+保持状态
│ 源件↔结果切换           │ -             │ OFF                   │ OFF                  │ 从头+保持状态
│ 用户暂停后切换          │ -             │ ON                    │ ON                   │ 恢复进度+自动播放
│ 视频播放结束后切换      │ -             │ ON                    │ ON                   │ 从头+自动播放
└────────────────────────────────────────────────────────────────┘
```

### 2.5 实现方案

#### 2.5.1 新增 VideoPlaybackController 组件

创建独立的播放状态管理组件，封装所有播放控制逻辑：

```qml
// qml/components/VideoPlaybackController.qml
QtObject {
    id: controller
    
    // ========== 属性 ==========
    property var mediaPlayer: null
    property bool isVideo: false
    property string currentSource: ""
    
    // 设置引用
    property bool autoPlayOnOpen: SettingsController.videoAutoPlay
    property bool autoPlayOnSwitch: SettingsController.videoAutoPlayOnSwitch
    property bool restorePosition: SettingsController.videoRestorePosition
    
    // ========== 内部状态 ==========
    property int _state: PlaybackState.Stopped
    property var _savedState: null
    property bool _isSourceResultSwitch: false
    property bool _userInteracted: false
    property int _loadRetryCount: 0
    
    // ========== 公共 API ==========
    function saveCurrentState() { ... }
    function restoreState() { ... }
    function handleSourceChange(newSource, isSourceResultSwitch) { ... }
    function handleUserPlayPause() { ... }
    function handleFileChange() { ... }
    
    // ========== 内部方法 ==========
    function _waitForMediaReady(callback, maxRetries) { ... }
    function _applyState(state) { ... }
    function _resetState() { ... }
}
```

#### 2.5.2 改进的媒体加载等待机制

```javascript
function _waitForMediaReady(callback, maxRetries) {
    if (!mediaPlayer) {
        console.warn("[VideoPlaybackController] No mediaPlayer")
        return
    }
    
    if (mediaPlayer.duration > 0) {
        callback()
        return
    }
    
    if (_loadRetryCount >= maxRetries) {
        console.warn("[VideoPlaybackController] Max retries reached")
        _loadRetryCount = 0
        return
    }
    
    _loadRetryCount++
    
    // 使用 Timer 延迟重试
    retryTimer.interval = 50 * _loadRetryCount  // 递增延迟
    retryTimer.callback = callback
    retryTimer.start()
}
```

#### 2.5.3 改进的状态恢复逻辑

```javascript
function restoreState() {
    if (!_savedState || !mediaPlayer) {
        console.log("[VideoPlaybackController] No saved state or mediaPlayer")
        return
    }
    
    _waitForMediaReady(function() {
        var shouldRestorePosition = restorePosition && _savedState.position > 0
        var shouldAutoPlay = autoPlayOnSwitch || _savedState.playbackState === PlaybackState.Playing
        
        // 检查视频是否已结束
        if (_savedState.position >= mediaPlayer.duration - 500) {
            shouldRestorePosition = false
            shouldAutoPlay = autoPlayOnSwitch  // 结束后是否重新播放
        }
        
        // 恢复进度
        if (shouldRestorePosition) {
            mediaPlayer.position = Math.min(_savedState.position, mediaPlayer.duration)
        } else {
            mediaPlayer.position = 0
        }
        
        // 恢复播放速率
        mediaPlayer.playbackRate = _savedState.playbackRate || 1.0
        
        // 恢复播放状态
        if (shouldAutoPlay && !_userInteracted) {
            mediaPlayer.play()
        } else if (_savedState.playbackState === PlaybackState.Paused) {
            mediaPlayer.pause()
        }
        
        // 重置状态
        _isSourceResultSwitch = false
        _loadRetryCount = 0
    }, 5)  // 最多重试5次
}
```

#### 2.5.4 用户交互检测

```javascript
function handleUserPlayPause() {
    _userInteracted = true
    
    // 重置用户交互标记（延迟）
    userInteractionResetTimer.restart()
    
    // 执行播放/暂停
    if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
        mediaPlayer.pause()
    } else {
        mediaPlayer.play()
    }
}

Timer {
    id: userInteractionResetTimer
    interval: 2000  // 2秒后重置
    onTriggered: _userInteracted = false
}
```

### 2.6 进度持久化方案

#### 2.6.1 数据存储结构

```cpp
// VideoProgressData 结构
struct VideoProgressData {
    QString filePath;           // 视频文件路径
    qint64 position;            // 播放位置 (ms)
    qint64 duration;            // 视频时长 (ms)
    double playbackRate;        // 播放速率
    qint64 lastPlayedTime;      // 最后播放时间戳
    int playbackState;          // 播放状态
};
```

#### 2.6.2 C++ 端实现

```cpp
// include/EnhanceVision/services/VideoProgressService.h
class VideoProgressService : public QObject {
    Q_OBJECT
    
public:
    static VideoProgressService* instance();
    
    Q_INVOKABLE void saveProgress(const QString& filePath, qint64 position, 
                                   qint64 duration, double rate, int state);
    Q_INVOKABLE QVariantMap loadProgress(const QString& filePath);
    Q_INVOKABLE void clearProgress(const QString& filePath);
    Q_INVOKABLE void clearAllProgress();
    
signals:
    void progressSaved(const QString& filePath);
    void progressLoaded(const QString& filePath, qint64 position);
    
private:
    QString m_progressFilePath;
    QJsonObject m_progressCache;
    QTimer* m_saveTimer;
    mutable QMutex m_mutex;
};
```

#### 2.6.3 QML 端集成

```javascript
// 在 EmbeddedMediaViewer.qml 中
VideoProgressService {
    id: progressService
}

// 保存进度（在关闭或切换时）
function saveProgressToStorage() {
    if (!isVideo || !currentFile || !currentFile.filePath) return
    
    progressService.saveProgress(
        currentFile.filePath,
        _mediaPlayer.position,
        _mediaPlayer.duration,
        _mediaPlayer.playbackRate,
        _mediaPlayer.playbackState
    )
}

// 加载进度（在打开时）
function loadProgressFromStorage() {
    if (!isVideo || !currentFile || !currentFile.filePath) return
    
    var progress = progressService.loadProgress(currentFile.filePath)
    if (progress.valid && restorePositionEnabled) {
        _savedPosition = progress.position
        _savedPlaybackRate = progress.playbackRate
        _savedPlaybackState = progress.playbackState
    }
}
```

### 2.7 异常处理增强

```javascript
// 错误类型枚举
const PlaybackError = {
    None: 0,
    MediaLoadFailed: 1,
    InvalidMedia: 2,
    NetworkError: 3,
    DecodeError: 4,
    Timeout: 5
}

// 错误处理器
function handlePlaybackError(errorType, errorMessage) {
    console.error("[VideoPlaybackController] Error:", errorType, errorMessage)
    
    switch (errorType) {
        case PlaybackError.MediaLoadFailed:
        case PlaybackError.InvalidMedia:
            // 显示错误提示
            errorDialog.show(qsTr("无法加载视频"), errorMessage)
            _resetState()
            break
            
        case PlaybackError.NetworkError:
            // 网络错误，尝试重连
            retryLoading()
            break
            
        case PlaybackError.Timeout:
            // 超时，提示用户
            errorDialog.show(qsTr("加载超时"), qsTr("视频加载超时，请检查网络连接"))
            break
    }
    
    // 记录日志
    Logger.logError("VideoPlayback", errorType, errorMessage, {
        source: currentSource,
        state: _state,
        savedState: _savedState
    })
}
```

---

## 三、实施步骤

### 阶段一：核心状态管理重构 (优先级: 高)

1. **创建 VideoPlaybackController.qml**
   - 实现状态机核心逻辑
   - 实现状态保存/恢复机制
   - 实现用户交互检测

2. **重构 EmbeddedMediaViewer.qml**
   - 集成 VideoPlaybackController
   - 移除冗余的状态管理代码
   - 更新事件处理逻辑

3. **重构 MediaViewerWindow.qml**
   - 应用相同的重构方案
   - 确保两个组件行为一致

### 阶段二：进度持久化实现 (优先级: 中)

1. **创建 VideoProgressService (C++)**
   - 实现进度数据存储
   - 实现 JSON 文件读写
   - 添加线程安全保护

2. **集成到 QML**
   - 在视频关闭时保存进度
   - 在视频打开时恢复进度
   - 添加进度清理机制

### 阶段三：异常处理增强 (优先级: 中)

1. **添加错误检测和处理**
   - 媒体加载失败处理
   - 网络错误处理
   - 超时处理

2. **添加日志记录**
   - 关键操作日志
   - 错误日志
   - 性能日志

### 阶段四：测试与验证 (优先级: 高)

1. **功能测试**
   - 各种状态组合测试
   - 边界条件测试
   - 异常场景测试

2. **性能测试**
   - 切换响应时间
   - 内存占用
   - CPU 使用率

---

## 四、技术要点

### 4.1 线程安全保障

- 使用 QMutex 保护共享状态
- 使用 QMetaObject::invokeMethod 进行跨线程调用
- 避免在信号处理器中直接修改状态

### 4.2 内存管理

- 使用 QPointer 管理 QObject 引用
- 及时断开信号连接
- 避免循环引用

### 4.3 性能优化

- 使用节流机制控制状态保存频率
- 避免频繁的磁盘 I/O
- 使用缓存减少重复计算

---

## 五、验收标准

### 5.1 功能验收

| 功能 | 验收标准 |
|------|----------|
| 源/结自动播放 | 开启时，切换源件/结果后自动播放，延迟 < 100ms |
| 源/结恢复进度 | 开启时，切换后进度恢复误差 < 0.5秒 |
| 开/切自动播放 | 开启时，打开新视频或切换文件自动播放 |
| 状态冲突处理 | 按优先级正确处理各种状态组合 |
| 用户操作优先 | 用户手动操作后，自动播放设置暂时失效 |
| 进度持久化 | 应用重启后可恢复上次观看进度 |

### 5.2 性能验收

| 指标 | 目标值 |
|------|--------|
| 切换响应时间 | < 100ms |
| 内存增长 | < 1MB/小时 |
| CPU 占用 | < 5% (空闲时) |

### 5.3 稳定性验收

- 无内存泄漏
- 无崩溃
- 无死锁
- 异常场景可恢复

---

## 六、风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| MediaPlayer API 变化 | 高 | 封装抽象层，隔离 Qt 版本差异 |
| 状态机复杂度增加 | 中 | 详细文档，单元测试覆盖 |
| 性能回归 | 中 | 性能基准测试，持续监控 |
| 向后兼容 | 低 | 保留旧 API，渐进式迁移 |

---

## 七、附录

### A. 相关文件清单

| 文件 | 修改类型 |
|------|----------|
| qml/components/VideoPlaybackController.qml | 新增 |
| qml/components/EmbeddedMediaViewer.qml | 重构 |
| qml/components/MediaViewerWindow.qml | 重构 |
| src/services/VideoProgressService.cpp | 新增 |
| include/EnhanceVision/services/VideoProgressService.h | 新增 |
| src/controllers/SettingsController.cpp | 扩展 |

### B. 参考资料

- Qt Multimedia 文档
- Qt State Machine Framework
- QML Best Practices
