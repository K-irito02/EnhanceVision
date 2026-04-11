# 媒体查看器运行时告警修复记录（2026-04-11）

## 概述

本次修复针对重构后媒体查看器链路中的运行时告警，目标是在不改变既有业务语义的前提下，消除会污染日志与干扰排障的 QML 警告。

**创建日期**: 2026-04-11

---

## 变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/mediaViewer/MediaViewerControls.qml` | 修复 `ResponsiveUtils` 访问方式、修复 `Menu` 动态插入 API、替换不存在图标名 |
| `qml/components/mediaViewer/MediaViewerCanvas.qml` | 在非图片场景将 `Image.source` 绑定为空，避免 mp4 被 `Image` 误加载 |
| `qml/components/mediaViewer/MediaViewerThumbnailBar.qml` | 将信号处理改为显式函数参数，消除参数注入弃用告警 |
| `qml/components/MessageList.qml` | 删除冗余 `console.log` 调试输出 |
| `CMakeLists.txt` | 补齐 `qml/utils/ResponsiveUtils.qml`、`qml/utils/qmldir`、`VideoControlBar.qml` 注册 |
| `resources/qml.qrc` | 补齐 `qml/utils/qmldir` 与 `NotificationManager.qml` 资源打包 |
| `.gitignore` | 显式忽略 `logs/` 与 `*.log` |

---

## 修复点与原因

### 1) `ResponsiveUtils` 未定义

- 原因：单例在相对目录导入场景下使用了未限定名访问。
- 修复：改为 `import "../../utils" as Utils`，并使用 `Utils.ResponsiveUtils`。

### 2) `Menu` 动态项参数类型错误

- 原因：`MenuItem` 被错误地通过 `insertMenu()` 注入，触发 C++ 参数转换错误。
- 修复：改用 `insertItem()` / `removeItem()`。

### 3) `QQuickImage` 误加载 mp4

- 原因：`Image.visible` 为 `false` 时仍会解析 `source`。
- 修复：在视频场景将 `source` 显式绑定为 `""`。

### 4) 信号参数注入弃用告警

- 原因：使用了旧式 `onActivated: ... origIndex ...` 语法。
- 修复：改为 `onActivated: function(origIndex) { ... }`。

### 5) 冗余调试输出

- 删除了消息列表代理中的调试 `console.log`，减少运行时噪声。

---

## 验证结果

- Release 构建通过。
- 启动过程 `stderr` 仅保留 NCNN GPU 能力输出，未再出现本次修复目标中的 QML 类型/参数/图标告警。

---

## 后续建议

- 继续逐步收敛 C++ 高频 `[INFO]/[DEBUG]` 输出到可配置日志级别（例如 Release 默认仅 `WARN+`）。
