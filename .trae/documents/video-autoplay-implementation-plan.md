# 视频自动播放功能实现计划

## 需求概述

为多媒体文件放大查看功能（嵌入式和独立式）实现视频自动播放功能：
- 从其他多媒体文件切换到视频文件时自动播放
- 初始点击视频文件进行放大查看时自动播放
- 提供自动播放开关配置，支持本地持久化
- 支持亮暗主题和中英文国际化

## 现有架构分析

### 相关文件
| 文件 | 用途 |
|------|------|
| `qml/components/MediaViewerWindow.qml` | 独立式媒体查看窗口 |
| `qml/components/EmbeddedMediaViewer.qml` | 嵌入式媒体查看窗口 |
| `src/controllers/SettingsController.cpp` | 设置控制器（单例） |
| `include/EnhanceVision/controllers/SettingsController.h` | 设置控制器头文件 |
| `qml/pages/SettingsPage.qml` | 设置页面 |
| `resources/i18n/app_zh_CN.ts` | 中文翻译 |
| `resources/i18n/app_en_US.ts` | 英文翻译 |

### 现有机制
- **设置存储**：`SettingsController` 使用 `QSettings` 存储配置到 `EnhanceVision/settings.ini`
- **国际化**：使用 `qsTr()` 函数，翻译文件为 `.ts` 格式
- **主题**：通过 `Theme` 单例管理，支持 `isDark` 属性判断亮暗模式
- **视频播放**：使用 `MediaPlayer` 组件，当前需手动点击播放

## 实现方案

### 1. SettingsController 扩展

#### 1.1 新增属性（SettingsController.h）
```cpp
Q_PROPERTY(bool videoAutoPlay READ videoAutoPlay WRITE setVideoAutoPlay NOTIFY videoAutoPlayChanged)
```

#### 1.2 新增成员变量和方法
```cpp
// 成员变量
bool m_videoAutoPlay;

// 访问器
bool videoAutoPlay() const;
void setVideoAutoPlay(bool autoPlay);

// 信号
void videoAutoPlayChanged();
```

#### 1.3 持久化处理
- 在 `saveSettings()` 中添加：`m_settings->setValue("video/autoPlay", m_videoAutoPlay);`
- 在 `loadSettings()` 中添加：`m_videoAutoPlay = m_settings->value("video/autoPlay", true).toBool();`
- 默认值设为 `true`（自动播放开启）

### 2. MediaViewerWindow.qml 修改（独立式窗口）

#### 2.1 添加自动播放属性绑定
```qml
property bool autoPlayEnabled: SettingsController.videoAutoPlay
```

#### 2.2 视频源变化时自动播放逻辑
在 `onCurrentSourceChanged` 信号处理器中添加自动播放逻辑：
```qml
function onCurrentSourceChanged() {
    if (isVideo && currentSource && currentSource !== "") {
        var src = currentSource
        if (!src.startsWith("file:///") && !src.startsWith("qrc:/")) {
            src = "file:///" + src
        }
        videoPlayer.source = src
        
        // 自动播放逻辑
        if (autoPlayEnabled && videoPlayer.playbackState === MediaPlayer.StoppedState) {
            Qt.callLater(function() {
                videoPlayer.play()
            })
        }
    }
}
```

#### 2.3 切换到视频文件时自动播放
在 `onIsVideoChanged` 信号处理器中添加：
```qml
function onIsVideoChanged() {
    if (!isVideo) {
        videoPlayer.stop()
        videoPlayer.source = ""
        _videoEnded = false
    } else if (autoPlayEnabled && currentSource) {
        // 切换到视频时自动播放
        Qt.callLater(function() {
            videoPlayer.play()
        })
    }
}
```

#### 2.4 视频控制区域添加自动播放开关
在 `videoControls` 的控制栏中添加开关按钮：
```qml
Row {
    spacing: 4
    
    // 自动播放开关
    Rectangle {
        id: autoPlayBtn
        width: autoPlayText.implicitWidth + 16
        height: 26
        radius: 5
        color: SettingsController.videoAutoPlay 
               ? Theme.colors.primary 
               : (autoPlayMouse.containsMouse ? Qt.rgba(1,1,1,0.12) : Qt.rgba(1,1,1,0.06))
        
        Text {
            id: autoPlayText
            anchors.centerIn: parent
            text: qsTr("自动播放")
            color: SettingsController.videoAutoPlay ? "#FFFFFF" : Theme.colors.mediaControlTextMuted
            font.pixelSize: 11
        }
        
        MouseArea {
            id: autoPlayMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: SettingsController.videoAutoPlay = !SettingsController.videoAutoPlay
        }
        
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
```

### 3. EmbeddedMediaViewer.qml 修改（嵌入式窗口）

#### 3.1 添加自动播放属性
```qml
property bool autoPlayEnabled: SettingsController.videoAutoPlay
```

#### 3.2 视频源变化时自动播放
修改 `onCurrentSourceChanged` 处理器：
```qml
function onCurrentSourceChanged() {
    if (root.isVideo && root.currentSource) {
        if (vidPlayer.playbackState === MediaPlayer.PlayingState) {
            vidPlayer.stop()
        }
        vidPlayer.source = root._getSource(root.currentSource)
        
        // 自动播放
        if (autoPlayEnabled) {
            Qt.callLater(function() {
                vidPlayer.play()
            })
        }
    }
}
```

#### 3.3 切换到视频时自动播放
修改 `onIsVideoChanged` 处理器：
```qml
function onIsVideoChanged() {
    if (!root.isVideo && vidPlayer.playbackState !== MediaPlayer.StoppedState) {
        vidPlayer.stop()
        root._videoEnded = false
        root._isPlaying = false
    } else if (root.isVideo && autoPlayEnabled && root.currentSource) {
        Qt.callLater(function() {
            vidPlayer.play()
        })
    }
}
```

#### 3.4 VideoControlBar 组件添加自动播放开关
在 `VideoControlBar` 组件的控制按钮行中添加自动播放开关（与 MediaViewerWindow 类似）

### 4. 设置页面添加自动播放配置

#### 4.1 在 SettingsPage.qml 中添加新配置项
在"音频"配置区域下方添加"视频"配置区域：
```qml
Rectangle {
    Layout.fillWidth: true
    implicitHeight: videoCol.implicitHeight + 32
    color: Theme.colors.card
    border.width: 1
    border.color: Theme.colors.cardBorder
    radius: Theme.radius.lg

    ColumnLayout {
        id: videoCol
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        RowLayout {
            spacing: 8
            ColoredIcon { iconSize: 18; source: Theme.icon("video"); color: Theme.colors.icon }
            Text { text: qsTr("视频"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text { text: qsTr("自动播放"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }

            Switch {
                checked: SettingsController.videoAutoPlay
                onCheckedChanged: SettingsController.videoAutoPlay = checked
            }
        }
    }
}
```

### 5. 国际化支持

#### 5.1 需要翻译的文本
| 中文 | 英文 | 位置 |
|------|------|------|
| 自动播放 | Auto Play | 视频控制栏、设置页面 |
| 视频 | Video | 设置页面标题 |

#### 5.2 更新翻译文件
在 `app_zh_CN.ts` 和 `app_en_US.ts` 中添加相应翻译条目

### 6. 错误处理与日志

#### 6.1 视频加载失败处理
```qml
MediaPlayer {
    onErrorOccurred: function(error, errorString) {
        console.warn("[MediaViewer] Video playback error:", errorString)
        // 显示错误提示
    }
}
```

#### 6.2 自动播放失败处理
由于浏览器自动播放策略限制，自动播放可能失败：
```qml
onPlaybackStateChanged: {
    if (playbackState === MediaPlayer.PlayingState) {
        root._videoEnded = false
    }
}

// 播放失败时显示播放按钮让用户手动触发
```

### 7. 实现步骤

#### 步骤 1：扩展 SettingsController
1. 修改 `SettingsController.h` 添加 `videoAutoPlay` 属性
2. 修改 `SettingsController.cpp` 实现属性访问器和持久化

#### 步骤 2：修改 MediaViewerWindow.qml
1. 添加 `autoPlayEnabled` 属性绑定
2. 修改 `onCurrentSourceChanged` 添加自动播放逻辑
3. 修改 `onIsVideoChanged` 添加切换时自动播放
4. 在视频控制栏添加自动播放开关按钮

#### 步骤 3：修改 EmbeddedMediaViewer.qml
1. 添加 `autoPlayEnabled` 属性绑定
2. 修改 `onCurrentSourceChanged` 添加自动播放逻辑
3. 修改 `onIsVideoChanged` 添加切换时自动播放
4. 在 `VideoControlBar` 组件添加自动播放开关

#### 步骤 4：修改 SettingsPage.qml
1. 添加"视频"配置区域
2. 添加自动播放开关配置项

#### 步骤 5：更新国际化文件
1. 更新 `app_zh_CN.ts` 添加中文翻译
2. 更新 `app_en_US.ts` 添加英文翻译

#### 步骤 6：构建验证
1. 使用 `qt-build-and-fix` 技能构建项目
2. 验证功能正确性

## 测试场景

### 功能测试
1. **初始打开视频**：点击视频文件，验证自动播放
2. **文件切换**：从图片切换到视频，验证自动播放
3. **开关同步**：在视频控制栏切换开关，验证设置页面同步
4. **开关同步**：在设置页面切换开关，验证视频控制栏同步
5. **持久化**：关闭应用后重新打开，验证设置保持

### 主题测试
1. **亮色主题**：验证所有 UI 元素显示正常
2. **暗色主题**：验证所有 UI 元素显示正常
3. **主题切换**：动态切换主题，验证 UI 正确更新

### 国际化测试
1. **中文环境**：验证所有文本正确显示中文
2. **英文环境**：验证所有文本正确显示英文
3. **语言切换**：动态切换语言，验证文本正确更新

### 边界测试
1. **视频加载失败**：验证错误处理和用户提示
2. **快速切换文件**：验证状态管理正确性
3. **全屏模式**：验证自动播放在全屏模式下正常工作

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| 自动播放策略限制 | 使用 `Qt.callLater` 延迟播放，失败时显示播放按钮 |
| 状态同步问题 | 使用 `SettingsController` 单例确保状态一致 |
| 性能影响 | 自动播放仅在视频源变化时触发，不影响性能 |

## 文件变更清单

| 文件 | 变更类型 |
|------|----------|
| `include/EnhanceVision/controllers/SettingsController.h` | 修改 |
| `src/controllers/SettingsController.cpp` | 修改 |
| `qml/components/MediaViewerWindow.qml` | 修改 |
| `qml/components/EmbeddedMediaViewer.qml` | 修改 |
| `qml/pages/SettingsPage.qml` | 修改 |
| `resources/i18n/app_zh_CN.ts` | 修改 |
| `resources/i18n/app_en_US.ts` | 修改 |
