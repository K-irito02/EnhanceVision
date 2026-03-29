# 视频播放问题修复笔记

## 概述

**创建日期**: 2026-03-30  
**相关组件**: VideoPlaybackController, EmbeddedMediaViewer, MediaViewerWindow

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/VideoPlaybackController.qml` | 修复加载动画显示逻辑，优化自动播放执行顺序，移除调试日志 |
| `qml/components/EmbeddedMediaViewer.qml` | 修复进度条 NaN 问题，优化播放图标显示逻辑，移除调试日志 |
| `qml/components/MediaViewerWindow.qml` | 修复进度条 NaN 问题，优化播放图标显示逻辑，移除调试日志 |
| `qml/components/MessageItem.qml` | 移除性能调试日志 |
| `qml/components/MessageList.qml` | 移除性能调试日志 |

---

## 二、修复的问题特性

- ✅ **问题1**：源/结切换时正确显示加载动画
- ✅ **问题2**：开/切自动播放时视频自动播放
- ✅ **问题3**：删除自动播放时的暂停图标闪烁
- ✅ **问题4**：修复进度条 NaN 和冻结问题
- ✅ **问题5**：移除所有调试日志，保持代码整洁

---

## 三、技术实现细节

### 关键代码片段

#### 1. 加载动画显示逻辑修复

**文件**: `VideoPlaybackController.qml` 行 41
```qml
// 修改前：
readonly property bool isLoading: _switchMode === 2 && SettingsController.videoAutoPlayOnSwitch

// 修改后：
readonly property bool isLoading: _switchMode === 2
```

**说明**: 现在无论"源/结自动播放"开关状态如何，源/结切换时都会显示加载动画。

#### 2. 自动播放逻辑优化

**文件**: `VideoPlaybackController.qml` 行 163-174
```qml
// 源/结切换自动播放：先 play 再 seek（使用 Qt.callLater 避免图标闪烁）
if (SettingsController.videoAutoPlayOnSwitch) {
    mediaPlayer.play()
    
    if (needSeek) {
        Qt.callLater(function() {
            if (mediaPlayer) {
                mediaPlayer.position = targetPos
                progressRestored(targetPos)
            }
        })
    }
}
```

**文件**: `VideoPlaybackController.qml` 行 190-196
```qml
// 普通文件打开自动播放：使用 Qt.callLater 延迟执行
if (SettingsController.videoAutoPlay) {
    Qt.callLater(function() {
        if (mediaPlayer) {
            mediaPlayer.play()
        }
    })
}
```

#### 3. 进度条 NaN 问题修复

**文件**: `EmbeddedMediaViewer.qml` 行 1217-1235
```qml
// VideoControlBar 组件
id: videoControlBar

property int displayPosition: {
    // 源/结切换中且需要恢复进度时，使用冻结位置
    if (playbackController._switchMode === 2 && SettingsController.videoRestorePosition) {
        return playbackController.frozenPosition || 0
    }
    // 否则使用实际播放位置（确保 videoPlayer 存在且位置有效）
    if (videoPlayer && videoPlayer.position !== undefined && videoPlayer.position >= 0) {
        return videoPlayer.position
    }
    return 0
}
```

**文件**: `EmbeddedMediaViewer.qml` 行 284-292
```qml
function _formatTime(ms) { 
    if (ms === undefined || ms === null || isNaN(ms) || ms < 0) return "00:00"
    var s = Math.floor(ms/1000)
    var h = Math.floor(s/3600)
    var m = Math.floor((s%3600)/60)
    var sec = s%60
    if (h > 0) return h.toString() + ":" + m.toString().padStart(2,'0') + ":" + sec.toString().padStart(2,'0')
    return Math.floor(s/60).toString().padStart(2,'0') + ":" + (s%60).toString().padStart(2,'0') 
}
```

#### 4. 播放图标闪烁优化

**文件**: `EmbeddedMediaViewer.qml` 行 1050-1061
```qml
// 当"开/切自动播放"开启时，隐藏切换过程中的播放图标，避免短暂闪烁
property bool _shouldShowPlayIcon: {
    if (!viewer.isVideo || !videoPlayer) return false
    if (playbackController.isLoading) return false
    if (videoPlayer.playbackState === MediaPlayer.PlayingState) return false
    // 当"开/切自动播放"开启且处于普通文件打开模式时，不显示播放图标
    if (SettingsController.videoAutoPlay && playbackController._switchMode === 1) return false
    return true
}
```

### 设计决策

1. **加载动画逻辑**：将 `isLoading` 条件简化为仅依赖 `_switchMode === 2`，确保源/结切换时始终显示加载动画。

2. **自动播放时序**：使用 `Qt.callLater()` 延迟执行播放操作，确保播放状态优先更新，避免UI显示暂停图标。

3. **进度条健壮性**：添加 `undefined` 和 `null` 检查，确保在所有情况下都返回有效值。

4. **播放图标条件**：增加更精确的显示条件判断，避免自动播放时的图标闪烁。

5. **代码整洁**：移除所有调试日志，保持代码简洁。

### 数据流

```
用户操作 → VideoPlaybackController → MediaPlayer → UI更新
    ↓           ↓                    ↓          ↓
源/结切换 → _switchMode=2 → isLoading=true → 显示加载动画
    ↓           ↓                    ↓          ↓
媒体就绪 → _executeDelayedAction → Qt.callLater(play) → 隐藏加载动画
```

---

## 四、遇到的问题及解决方案

### 问题1：加载动画显示条件错误
**现象**: 源/结切换时不显示加载动画
**原因**: `isLoading` 条件包含了 `SettingsController.videoAutoPlayOnSwitch` 检查
**解决**: 简化条件为 `_switchMode === 2`

### 问题2：自动播放时暂停图标闪烁
**现象**: 开启自动播放时短暂显示暂停图标
**原因**: 播放和 seek 操作的时序问题
**解决**: 使用 `Qt.callLater()` 延迟执行操作，优化播放状态更新顺序

### 问题3：进度条显示 NaN:NaN
**现象**: 进度条显示 NaN:NaN 且不移动
**原因**: `videoPlayer` 在初始化时可能为 `null` 或 `undefined`，且 `parent.parent.displayPosition` 引用错误
**解决**: 添加空值检查和默认值处理，使用 `id` 直接引用属性

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 源/结切换 + 自动播放关闭 | 显示加载动画 → 显示暂停图标 | ✅ 通过 |
| 最初打开视频 + 开/切自动播放开启 | 视频自动播放 | ✅ 通过 |
| 导航切换 + 开/切自动播放开启 | 不显示暂停图标 | ✅ 通过 |
| 进度条显示 | 正常显示时间和进度 | ✅ 通过 |

---

## 六、后续工作

- [x] 移除所有调试日志
- [ ] 持续监控播放稳定性
- [ ] 优化视频切换性能
