# 视频自动播放功能增强实现计划

## 问题分析

### 核心问题：点击"源件/结果"按钮后视频暂停且位置重置

**问题根源定位**：

在 [MediaViewerWindow.qml](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/MediaViewerWindow.qml) 中：

1. **播放状态保存逻辑** (第111-127行)：当 `_videoEnded` 为 true 时，保存的位置会被重置为 0
2. **状态恢复时机**：依赖 `onMediaStatusChanged` 中的 `Qt.callLater`，可能存在时序问题
3. **切换标识处理**：`_isSwitchingSource` 标识在某些边界情况下可能未正确重置

**现有自动播放功能分析**：

| 功能 | 位置 | 作用 |
|------|------|------|
| 视频控制栏"自动播放"按钮 | MediaViewerWindow.qml:1203-1235 | 控制视频打开时是否自动播放 |
| 设置页面"自动播放"开关 | SettingsPage.qml:407-429 | 同上，全局配置 |
| SettingsController.videoAutoPlay | SettingsController.h:34 | 后端持久化属性 |

**问题**：现有功能命名不够清晰，无法区分"打开视频自动播放"和"切换源件/结果后自动播放"两种场景。

---

## 实现方案

### 功能设计

#### 1. 自动播放功能分类

| 功能 | 原名称 | 新名称 | 作用场景 |
|------|--------|--------|----------|
| 功能A | 自动播放 | 打开自动播放 | 打开新视频/切换视频时是否自动播放 |
| 功能B | (新增) | 切换自动播放 | 点击"源件/结果"按钮切换后是否自动播放 |
| 功能C | (新增) | 恢复播放位置 | 切换源件/结果时是否恢复之前的播放位置 |

#### 2. UI布局设计

**视频控制栏布局**（在现有"自动播放"按钮旁）：
```
[打开自动播放] [切换自动播放] [恢复播放位置]
```

**设置页面布局**（视频设置区域）：
```
视频
├── 打开自动播放    [开关]
├── 切换自动播放    [开关]  
└── 恢复播放位置    [开关]
```

#### 3. 状态管理

- 三个功能相互独立，可分别开启/关闭
- 所有状态通过 SettingsController 持久化
- UI 控件与设置菜单双向同步

---

## 实现步骤

### 阶段一：后端设置控制器扩展

**文件**：`include/EnhanceVision/controllers/SettingsController.h`

1. 添加新属性声明：
   - `videoAutoPlayOnSwitch`：切换源件/结果后自动播放
   - `videoRestorePosition`：切换时恢复播放位置

2. 添加对应的信号：
   - `videoAutoPlayOnSwitchChanged()`
   - `videoRestorePositionChanged()`

**文件**：`src/controllers/SettingsController.cpp`

1. 添加成员变量初始化
2. 实现属性访问器
3. 更新 `saveSettings()` 和 `loadSettings()` 方法
4. 更新 `resetToDefaults()` 方法

### 阶段二：修复核心问题

**文件**：`qml/components/MediaViewerWindow.qml`

1. **修复播放状态保存逻辑**：
   - 改进 `_savePlaybackState()` 函数，确保正确保存当前播放状态
   - 处理 `_videoEnded` 边界情况

2. **修复播放状态恢复逻辑**：
   - 改进 `_restorePlaybackState()` 函数
   - 根据新配置决定是否恢复位置和是否自动播放

3. **优化切换流程**：
   - 确保 `_isSwitchingSource` 标识正确管理
   - 添加更可靠的状态恢复机制

### 阶段三：UI功能扩展

**文件**：`qml/components/MediaViewerWindow.qml`

1. **重命名现有按钮**：
   - "自动播放" → "打开自动播放"
   - 添加 tooltip 提示

2. **新增功能按钮**：
   - "切换自动播放" 按钮
   - "恢复播放位置" 按钮
   - 添加 tooltip 提示

3. **实现双向同步**：
   - 按钮状态与 SettingsController 属性绑定
   - 添加 Connections 监听属性变化

**文件**：`qml/pages/SettingsPage.qml`

1. 在视频设置区域添加新配置项
2. 实现与 MediaViewerWindow 的双向同步

### 阶段四：国际化支持

**文件**：`resources/i18n/app_zh_CN.ts` 和 `resources/i18n/app_en_US.ts`

添加新翻译字符串：

| 中文 | 英文 | 用途 |
|------|------|------|
| 打开自动播放 | Auto Play on Open | 功能A按钮 |
| 切换自动播放 | Auto Play on Switch | 功能B按钮 |
| 恢复播放位置 | Restore Position | 功能C按钮 |
| 打开新视频时自动播放 | Auto play when opening a new video | 功能A提示 |
| 切换源件/结果时自动播放 | Auto play when switching between source and result | 功能B提示 |
| 切换时恢复播放位置 | Restore playback position when switching | 功能C提示 |

### 阶段五：主题适配

确保所有新增 UI 元素：
- 使用 `Theme.colors.*` 颜色系统
- 支持 `Theme.isDark` 亮暗主题切换
- 动画效果与现有风格一致

---

## 详细代码修改清单

### 1. SettingsController.h

```cpp
// 新增属性声明
Q_PROPERTY(bool videoAutoPlayOnSwitch READ videoAutoPlayOnSwitch WRITE setVideoAutoPlayOnSwitch NOTIFY videoAutoPlayOnSwitchChanged)
Q_PROPERTY(bool videoRestorePosition READ videoRestorePosition WRITE setVideoRestorePosition NOTIFY videoRestorePositionChanged)

// 新增方法声明
bool videoAutoPlayOnSwitch() const;
void setVideoAutoPlayOnSwitch(bool autoPlay);
bool videoRestorePosition() const;
void setVideoRestorePosition(bool restore);

// 新增信号
void videoAutoPlayOnSwitchChanged();
void videoRestorePositionChanged();

// 新增成员变量
bool m_videoAutoPlayOnSwitch;
bool m_videoRestorePosition;
```

### 2. SettingsController.cpp

```cpp
// 构造函数初始化
, m_videoAutoPlayOnSwitch(true)
, m_videoRestorePosition(true)

// 属性访问器实现
bool SettingsController::videoAutoPlayOnSwitch() const { return m_videoAutoPlayOnSwitch; }
void SettingsController::setVideoAutoPlayOnSwitch(bool autoPlay) { /* 实现 */ }

bool SettingsController::videoRestorePosition() const { return m_videoRestorePosition; }
void SettingsController::setVideoRestorePosition(bool restore) { /* 实现 */ }

// saveSettings() 添加
m_settings->setValue("video/autoPlayOnSwitch", m_videoAutoPlayOnSwitch);
m_settings->setValue("video/restorePosition", m_videoRestorePosition);

// loadSettings() 添加
m_videoAutoPlayOnSwitch = m_settings->value("video/autoPlayOnSwitch", true).toBool();
m_videoRestorePosition = m_settings->value("video/restorePosition", true).toBool();

// resetToDefaults() 添加
m_videoAutoPlayOnSwitch = true;
m_videoRestorePosition = true;
```

### 3. MediaViewerWindow.qml 关键修改

```qml
// 属性绑定
property bool autoPlayOnSwitchEnabled: SettingsController.videoAutoPlayOnSwitch
property bool restorePositionEnabled: SettingsController.videoRestorePosition

// 修复 _savePlaybackState
function _savePlaybackState() {
    if (!mediaPlayer) return
    // 始终保存当前位置，不因 _videoEnded 重置
    _savedPosition = mediaPlayer.position
    _savedPlaybackRate = mediaPlayer.playbackRate
    // ... 保存播放状态
}

// 修复 _restorePlaybackState
function _restorePlaybackState() {
    if (!mediaPlayer || mediaPlayer.duration <= 0) return
    
    // 根据配置决定是否恢复位置
    if (restorePositionEnabled && _savedPosition > 0) {
        mediaPlayer.position = Math.min(_savedPosition, mediaPlayer.duration)
    }
    mediaPlayer.playbackRate = _savedPlaybackRate
    
    // 根据配置决定是否自动播放
    if (autoPlayOnSwitchEnabled && _savedPlaybackState === 1) {
        mediaPlayer.play()
    } else if (_savedPlaybackState === 2) {
        mediaPlayer.pause()
    }
}
```

### 4. SettingsPage.qml 视频设置区域

```qml
// 视频设置区域扩展
ColumnLayout {
    // ... 现有代码
    
    RowLayout {
        Text { text: qsTr("打开自动播放"); ... }
        Item { Layout.fillWidth: true }
        Switch {
            checked: SettingsController.videoAutoPlay
            onToggled: SettingsController.videoAutoPlay = checked
        }
    }
    
    RowLayout {
        Text { text: qsTr("切换自动播放"); ... }
        Item { Layout.fillWidth: true }
        Switch {
            checked: SettingsController.videoAutoPlayOnSwitch
            onToggled: SettingsController.videoAutoPlayOnSwitch = checked
        }
    }
    
    RowLayout {
        Text { text: qsTr("恢复播放位置"); ... }
        Item { Layout.fillWidth: true }
        Switch {
            checked: SettingsController.videoRestorePosition
            onToggled: SettingsController.videoRestorePosition = checked
        }
    }
}
```

---

## 测试计划

### 功能测试

1. **核心问题修复验证**：
   - 播放视频 → 点击"源件/结果"按钮 → 验证播放状态正确保持

2. **新功能测试**：
   - 测试"打开自动播放"开关
   - 测试"切换自动播放"开关
   - 测试"恢复播放位置"开关
   - 测试三个功能的组合使用

3. **持久化测试**：
   - 修改设置 → 重启应用 → 验证设置保持

4. **双向同步测试**：
   - 在视频控制栏修改 → 验证设置页面同步
   - 在设置页面修改 → 验证视频控制栏同步

5. **国际化测试**：
   - 切换语言 → 验证所有新文本正确翻译

6. **主题测试**：
   - 切换亮/暗主题 → 验证所有新 UI 元素正确显示

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 播放状态恢复时序问题 | 中 | 使用 Qt.callLater 确保媒体加载完成后再恢复 |
| 多个自动播放设置冲突 | 低 | 三个功能独立，互不影响 |
| 性能影响 | 低 | 仅在切换时触发，无持续性能开销 |

---

## 预计工作量

| 阶段 | 预计时间 |
|------|----------|
| 后端设置控制器扩展 | 30分钟 |
| 核心问题修复 | 45分钟 |
| UI功能扩展 | 60分钟 |
| 国际化支持 | 20分钟 |
| 测试验证 | 30分钟 |
| **总计** | **约3小时** |
