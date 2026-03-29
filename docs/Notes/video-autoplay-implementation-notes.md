# 视频自动播放功能实现笔记

## 概述

为多媒体文件放大查看功能（嵌入式和独立式）实现视频自动播放功能，包括自动播放开关配置、本地持久化、主题适配和国际化支持。

**创建日期**: 2026-03-29
**作者**: AI Assistant

---

## 一、变更概述

### 1. 新增文件

无新增文件

### 2. 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `include/EnhanceVision/controllers/SettingsController.h` | 新增 `videoAutoPlay` 属性声明 |
| `src/controllers/SettingsController.cpp` | 实现 `videoAutoPlay` 属性和持久化 |
| `qml/components/MediaViewerWindow.qml` | 添加自动播放逻辑和控制栏开关按钮 |
| `qml/components/EmbeddedMediaViewer.qml` | 添加自动播放逻辑和控制栏开关按钮 |
| `qml/pages/SettingsPage.qml` | 添加视频配置区域和自动播放开关 |
| `resources/i18n/app_zh_CN.ts` | 添加中文翻译条目 |
| `resources/i18n/app_en_US.ts` | 添加英文翻译条目 |

---

## 二、实现的功能特性

- ✅ 视频自动播放：从图片切换到视频、初始打开视频时自动播放
- ✅ 配置开关：视频控制栏和设置页面双向同步的自动播放开关
- ✅ 本地持久化：通过 QSettings 保存用户偏好设置
- ✅ 主题适配：亮暗主题下 UI 元素正确显示
- ✅ 国际化：中英文翻译支持

---

## 三、技术实现细节

### SettingsController 扩展

```cpp
// 属性声明
Q_PROPERTY(bool videoAutoPlay READ videoAutoPlay WRITE setVideoAutoPlay NOTIFY videoAutoPlayChanged)

// 成员变量
bool m_videoAutoPlay;

// 持久化
m_settings->setValue("video/autoPlay", m_videoAutoPlay);
m_videoAutoPlay = m_settings->value("video/autoPlay", true).toBool();
```

### QML 自动播放逻辑

```qml
// 视频源变化时自动播放
function onCurrentSourceChanged() {
    if (isVideo && currentSource && currentSource !== "") {
        videoPlayer.source = src
        if (autoPlayEnabled && videoPlayer.playbackState === MediaPlayer.StoppedState) {
            Qt.callLater(function() { videoPlayer.play() })
        }
    }
}

// 切换到视频时自动播放
function onIsVideoChanged() {
    if (!isVideo) {
        videoPlayer.stop()
        videoPlayer.source = ""
    } else if (autoPlayEnabled && currentSource) {
        Qt.callLater(function() { videoPlayer.play() })
    }
}
```

### 设计决策

1. **默认开启自动播放**：用户体验考量，默认开启自动播放更符合用户预期
2. **使用 Qt.callLater**：延迟播放调用，避免状态竞争问题
3. **单例状态管理**：通过 SettingsController 单例确保两处开关状态一致

---

## 四、遇到的问题及解决方案

### 问题 1：设置页面开关无法控制视频控制栏按钮

**现象**：在设置页面切换自动播放开关，视频控制栏中的按钮状态不同步

**原因**：自定义 Switch 控件没有 `pressed` 属性，但代码中使用了 `pressed` 来判断是否是用户交互

**解决方案**：改用 `onToggled` 信号处理用户交互，避免使用不存在的 `pressed` 属性

**代码示例**：
```qml
// 错误写法
Switch {
    onCheckedChanged: {
        if (!updating && pressed) {  // pressed 属性不存在
            SettingsController.videoAutoPlay = checked
        }
    }
}

// 正确写法
Switch {
    checked: SettingsController.videoAutoPlay
    onToggled: {
        SettingsController.videoAutoPlay = checked
    }
}
```

---

## 五、测试验证

### 测试场景

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 初始打开视频 | 自动播放 | ✅ 通过 |
| 从图片切换到视频 | 自动播放 | ✅ 通过 |
| 设置页面切换开关 | 视频控制栏同步 | ✅ 通过 |
| 视频控制栏切换开关 | 设置页面同步 | ✅ 通过 |
| 关闭程序后重新打开 | 设置保持 | ✅ 通过 |
| 亮色主题 | UI 正确显示 | ✅ 通过 |
| 暗色主题 | UI 正确显示 | ✅ 通过 |
| 中文环境 | 文本正确显示 | ✅ 通过 |
| 英文环境 | 文本正确显示 | ✅ 通过 |

### 边界条件

- 视频加载失败：显示错误提示，不崩溃
- 快速切换文件：状态管理正确
- 全屏模式：自动播放正常工作

---

## 六、性能影响

| 指标 | 变更前 | 变更后 | 影响 |
|------|--------|--------|------|
| 内存占用 | 无变化 | 无变化 | 无影响 |
| 启动时间 | 无变化 | 无变化 | 无影响 |
| 配置读取 | 无 | 1 次 | 可忽略 |

---

## 七、后续工作

- [ ] 考虑添加视频播放失败时的重试机制
- [ ] 考虑添加视频预加载功能优化用户体验

---

## 八、参考资料

- Qt MediaPlayer 文档
- QSettings 持久化存储
- QML 属性绑定最佳实践
