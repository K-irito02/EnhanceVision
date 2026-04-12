# 视频进度条拖拽暂停优化

## 概述

优化视频播放时拖拽进度条的交互行为：拖拽期间视频暂停，松开后从目标位置恢复播放。

**创建日期**: 2026-04-13

---

## 变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/mediaViewer/MediaViewerControls.qml` | 拆分 displayPosition、添加拖拽暂停/释放恢复逻辑、条件绑定 |

---

## 实现的功能特性

- ✅ 拖拽进度条时视频自动暂停
- ✅ 拖拽过程中（鼠标按下状态）视频保持暂停
- ✅ 松开鼠标后从目标位置恢复播放（若之前在播放）
- ✅ 暂停状态下拖拽进度条，松开后保持暂停
- ✅ 拖拽期间时间标签实时更新（跟随滑块位置而非播放器位置）
- ✅ 拖拽期间可 scrubbing（帧预览），视频不播放
- ✅ 源/结切换期间拖拽被安全拦截

---

## 技术实现细节

### 根因分析

1. **拖拽时视频继续播放**：`onMoved` 只 seek 不暂停，播放状态下 position 持续被覆盖
2. **绑定冲突**：`value: root.displayPosition` 在拖拽期间仍生效，player position 异步更新拉回滑块
3. **时间显示不准**：`displayPosition` 跟随 `videoPlayer.position`（有延迟），而非滑块实际位置

### 设计方案

1. **拆分 `_playerPosition` 和 `displayPosition`**
   - `_playerPosition`：播放器真实位置（含 frozenPosition 逻辑）
   - `displayPosition`：拖拽时取滑块值，否则取 `_playerPosition`

2. **条件绑定 `Binding on value`**
   - `when: !seekSlider.pressed`：拖拽期间断开绑定，避免冲突
   - 非拖拽时绑定到 `_playerPosition`，正常跟随播放进度

3. **按下暂停 / 释放恢复**
   - `onPressedChanged`：按下时保存 `_wasPlaying` 并暂停，释放时 seek + 恢复播放
   - 释放时检查 `isLoading`，避免与源/结切换逻辑冲突

4. **拖拽中 scrubbing**
   - `onMoved` 仍然 seek（视频已暂停，不会冲突），提供帧预览反馈

### 关键代码

```qml
readonly property int _playerPosition: {
    if (root.playbackController && root.playbackController._switchMode === 2
        && SettingsController.videoRestorePosition) {
        return root.playbackController.frozenPosition || 0
    }
    if (root.videoPlayer && root.videoPlayer.position !== undefined
        && root.videoPlayer.position >= 0) {
        return root.videoPlayer.position
    }
    return 0
}

readonly property int displayPosition: seekSlider.pressed
    ? Math.round(seekSlider.value) : root._playerPosition

Slider {
    id: seekSlider
    property bool _wasPlaying: false

    onPressedChanged: {
        if (!root.videoPlayer) return
        if (pressed) {
            _wasPlaying = root.videoPlayer.playbackState === MediaPlayer.PlayingState
            root.videoPlayer.pause()
        } else {
            if (root.playbackController && root.playbackController.isLoading) return
            root.videoPlayer.position = Math.round(value)
            if (_wasPlaying) root.videoPlayer.play()
        }
    }

    onMoved: {
        if (root.videoPlayer && (!root.playbackController || !root.playbackController.isLoading)) {
            root.videoPlayer.position = Math.round(value)
        }
    }

    Binding on value {
        value: root._playerPosition
        when: !seekSlider.pressed
    }
}
```

---

## 测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 播放中拖拽进度条 | 视频暂停，滑块流畅移动 | ✅ 通过 |
| 拖拽中松开鼠标 | 从新位置恢复播放 | ✅ 通过 |
| 暂停中拖拽进度条 | 松开后保持暂停 | ✅ 通过 |
| 拖拽时时间标签 | 跟随滑块位置实时更新 | ✅ 通过 |
| 进度条无跳动 | 拖拽流畅无回弹 | ✅ 通过 |
