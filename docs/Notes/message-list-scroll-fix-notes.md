# 消息列表滚动位置修复 - 完整解决方案

## 概述

**创建日期**: 2026-03-31  
**问题描述**: 会话切换时消息列表滚动位置不正确，以及快速滚动时卡片收缩

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `include/EnhanceVision/models/MessageModel.h` | 添加 `modelAboutToReset` 和 `modelResetCompleted` 信号 |
| `src/models/MessageModel.cpp` | 在模型重置前后发出信号，包括初始加载场景 |
| `qml/components/MessageList.qml` | 重构滚动位置保存/恢复逻辑，优化 ListView 性能 |

---

## 二、实现的功能特性

- ✅ **会话切换位置保持**：切换会话时保持用户之前的滚动位置
- ✅ **首次访问显示底部**：首次访问任何会话时自动显示底部最新消息
- ✅ **无滚动动画**：会话切换时位置固定，无可见滚动效果
- ✅ **用户交互检测**：只有用户主动滚动过的会话才保存位置
- ✅ **ListView 性能优化**：增加缓存区域，消除卡片收缩和滚动卡顿

---

## 三、技术实现细节

### 1. C++ 信号机制

在 `MessageModel` 中添加两个信号，精确控制 QML 端的滚动位置保存和恢复时机：

```cpp
// MessageModel.h
signals:
    void modelAboutToReset();  // 模型即将重置
    void modelResetCompleted(); // 模型重置完成

// MessageModel.cpp - setMessages()
bool needsReset = (oldCount == 0 && newCount > 0);  // 初始加载
if (!needsReset) {
    for (int i = 0; i < commonCount; ++i) {
        if (m_messages[i].id != messages[i].id) {
            needsReset = true;
            break;
        }
    }
}

if (needsReset) {
    emit modelAboutToReset();
    beginResetModel();
    m_messages = messages;
    endResetModel();
    emit modelResetCompleted();
    // ...
}
```

### 2. QML 滚动位置管理

使用 `userHasInteracted` 标记区分"用户主动滚动过"和"首次访问"：

```qml
// 标记用户是否在当前会话中主动滚动过
property bool userHasInteracted: false

// 当用户主动滚动时标记已交互
onMovingChanged: {
    if (moving && !isSessionSwitching) {
        userHasInteracted = true
    }
}

// 模型即将重置：只有用户主动滚动过才保存位置
function onModelAboutToReset() {
    if (root.currentSessionId !== "" && messageList.userHasInteracted) {
        root._sessionScrollPositions[root.currentSessionId] = {
            contentY: messageList.contentY - messageList.originY,
            userHasScrolledUp: messageList.userHasScrolledUp,
            userHasInteracted: true
        }
    }
    messageList.isSessionSwitching = true
}

// 模型重置完成：恢复滚动位置
Timer {
    id: restoreAfterResetTimer
    interval: 100
    onTriggered: {
        var savedPos = root._sessionScrollPositions[root.currentSessionId]
        if (savedPos !== undefined && savedPos.userHasInteracted) {
            // 恢复到用户之前滚动到的位置
            var targetY = Math.max(0, Math.min(savedPos.contentY, 
                messageList.contentHeight - messageList.height)) + messageList.originY
            messageList.contentY = targetY
        } else {
            // 用户从未在此会话中滚动过，显示底部最新消息
            messageList.positionViewAtEnd()
        }
        messageList.isSessionSwitching = false
    }
}
```

### 3. ListView 性能优化

```qml
ListView {
    // 性能优化：增加缓存区域，预加载更多项目避免滚动时卡片收缩
    cacheBuffer: 2000  // 缓存区域高度（像素）
    displayMarginBeginning: 500  // 在可见区域上方额外渲染
    displayMarginEnd: 500  // 在可见区域下方额外渲染
    reuseItems: true  // 启用项目复用以提高性能
    
    // 会话切换时隐藏内容，避免用户看到滚动过程
    opacity: isSessionSwitching ? 0 : 1
}
```

---

## 四、遇到的问题及解决方案

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 切换会话时滚动到顶部 | `beginResetModel()` 导致 ListView 重建 | 使用 C++ 信号精确控制保存/恢复时机 |
| 首次访问不在底部 | 初始加载不触发 `modelAboutToReset` | 检测 `oldCount == 0 && newCount > 0` 场景 |
| 切换时有滚动动画 | 恢复位置时 ListView 可见 | 使用 `opacity: 0` 隐藏切换过程 |
| 快速滚动卡片收缩 | `shouldLoadContent: !isScrolling` 阻止加载 | 改为始终加载，增加 cacheBuffer |
| 滚动卡顿 | 缓存区域不足 | 增加 cacheBuffer 和 displayMargin |

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 首次打开应用 | 所有会话显示底部最新消息 | ✅ 通过 |
| 切换会话（未滚动过） | 保持在底部 | ✅ 通过 |
| 切换会话（已滚动过） | 恢复到之前位置 | ✅ 通过 |
| 快速滚动 | 无卡片收缩 | ✅ 通过 |
| 滚动流畅性 | 无卡顿或突然移动 | ✅ 通过 |

---

## 六、关键设计决策

1. **使用 C++ 信号而非 QML 属性变化**：确保在模型重置前后精确执行保存/恢复操作
2. **区分用户交互和系统操作**：只保存用户主动滚动过的位置，避免保存错误的初始位置
3. **使用 Timer 延迟恢复**：确保 ListView 内容高度已计算完成后再恢复位置
4. **增加缓存区域**：牺牲少量内存换取流畅的滚动体验

---

## 七、后续工作

- [ ] 监控大量消息场景下的内存使用
- [ ] 考虑持久化滚动位置到本地存储
