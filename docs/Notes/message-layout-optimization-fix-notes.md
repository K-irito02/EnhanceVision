# 消息展示区域布局优化修复笔记

## 概述

**创建日期**: 2026-03-31  
**问题描述**: 修复"待处理预览区域"和"最小化停靠区域"与消息展示区域消息卡片的布局冲突问题

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/MessageList.qml` | 添加预览区域高度属性和监听逻辑，统一滚动行为 |
| `qml/pages/MainPage.qml` | 传递预览区域高度给消息列表组件 |

---

## 二、实现的功能特性

- ✅ **统一滚动逻辑**: 待处理预览区域和最小化停靠区域使用相同的消息上移逻辑
- ✅ **智能覆盖判断**: 根据最新消息完整显示状态决定是否允许覆盖
- ✅ **状态锁定机制**: 预览区域出现时锁定覆盖模式，避免状态切换冲突
- ✅ **延迟滚动**: 使用定时器确保布局计算完成后再执行滚动

---

## 三、技术实现细节

### 关键实现方案

#### 1. 预览区域高度传递
```qml
// MainPage.qml
MessageList {
    id: messageListView
    previewAreaHeight: pendingFilePreviewArea.height
}
```

#### 2. 滚动逻辑统一
```qml
// MessageList.qml
onPreviewAreaHeightChanged: {
    if (previewAreaHeight > 0) {
        // 预览区域出现时检查是否需要滚动
        if (root._overlayModeLocked && !root._lockedCanOverlay && messageList.count > 0) {
            previewScrollTimer.restart()
        }
    }
}
```

#### 3. 状态锁定机制
```qml
Connections {
    target: root
    function onHasFilesChanged() {
        if (root.hasFiles) {
            // 锁定当前覆盖模式
            root._lockedCanOverlay = root.canOverlayLastMessage
            root._overlayModeLocked = true
        } else {
            // 解锁覆盖模式
            root._overlayModeLocked = false
        }
    }
}
```

#### 4. 延迟滚动定时器
```qml
Timer {
    id: previewScrollTimer
    interval: 50  // 短延迟确保布局计算完成
    onTriggered: {
        if (messageList.count > 0) {
            messageList.scrollToBottomAnimated()
        }
    }
}
```

### 设计决策

1. **使用高度监听而非直接监听 hasFiles**: 确保在布局实际变化时触发滚动
2. **双重状态锁定**: `_overlayModeLocked` 和 `_lockedCanOverlay` 组合判断
3. **统一滚动方法**: 复用现有的 `scrollToBottomAnimated()` 方法

---

## 四、遇到的问题及解决方案

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 初始条件错误 | `!root._overlayModeLocked` 阻止了所需滚动 | 改为 `root._overlayModeLocked && !root._lockedCanOverlay` |
| 滚动时机不当 | 在 `hasFiles` 变化时立即触发滚动 | 移至 `previewAreaHeight` 变化时触发 |
| 状态切换冲突 | 预览区域出现时覆盖模式可能变化 | 添加状态锁定机制 |

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 最新消息完整显示时上传文件 | 消息列表上移，不被覆盖 | ✅ 通过 |
| 最新消息不完整显示时上传文件 | 消息可被覆盖 | ✅ 通过 |
| 预览区域消失后 | 覆盖模式解锁 | ✅ 通过 |
| 最小化停靠区域出现 | 与预览区域行为一致 | ✅ 通过 |
| 会话切换 | 状态正确重置 | ✅ 通过 |

---

## 六、后续工作

- [ ] 监控长期稳定性
- [ ] 考虑优化延迟时间
- [ ] 添加更多边界情况测试

---

## 七、相关文件

- `qml/components/MessageList.qml` - 消息列表组件
- `qml/pages/MainPage.qml` - 主页面布局
- `docs/Notes/ai-inference-crash-fix-notes.md` - 相关的AI推理修复笔记
