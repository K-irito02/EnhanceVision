# 修复计划：剩余时间后台静默倒计时

## 问题描述

当用户从其他会话标签切换回来时，消息卡片上的"剩余时间"会从0开始变化到最终剩余时间，而不是直接显示正确的剩余时间。

## 根本原因

1. **Timer 运行条件问题**：`elapsedTimer` 的 `running` 条件在 delegate 重新创建时可能短暂不满足
2. **动画行为问题**：`_displayRemainingSec` 有 800ms 的动画，恢复时会从旧值过渡到新值
3. **初始化时机问题**：`_remainingSec` 初始化为 `predictedTotalSec`，然后等待 Timer 触发才更新

## 解决方案

### 方案一：立即计算剩余时间（推荐）

在 `Component.onCompleted` 和 `onPersistedProcessingStartTimeChanged` 中，立即计算并设置 `_displayRemainingSec`，跳过动画。

**修改文件**：`qml/components/MessageItem.qml`

**具体步骤**：

1. 添加一个标志位 `_skipDisplayAnimation`，用于控制是否跳过动画
2. 在恢复处理开始时间时，先计算正确的剩余时间，然后直接设置 `_displayRemainingSec`
3. 在 `elapsedTimer.onTriggered` 中，如果是首次更新，也直接设置值

### 方案二：使用 Binding 替代 Behavior

将 `_displayRemainingSec` 的 `Behavior` 改为条件性的，只在需要时启用动画。

**修改文件**：`qml/components/MessageItem.qml`

**具体步骤**：

1. 添加属性 `_enableRemainingTimeAnimation`
2. 修改 `Behavior` 的启用条件
3. 在恢复时禁用动画，恢复后启用

## 推荐方案：方案一

方案一更简单直接，不需要改变动画架构，只需在关键时刻跳过动画。

## 实施步骤

### Step 1：添加跳过动画的标志位

在 `MessageItem.qml` 中添加：

```qml
// 是否跳过剩余时间动画（用于恢复时立即显示正确值）
property bool _skipRemainingTimeAnimation: false
```

### Step 2：修改 Behavior 条件

修改 `_displayRemainingSec` 的 Behavior：

```qml
Behavior on _displayRemainingSec {
    enabled: !_skipRemainingTimeAnimation
    NumberAnimation { duration: 800; easing.type: Easing.OutCubic }
}
```

### Step 3：修改恢复逻辑

在 `onPersistedProcessingStartTimeChanged` 和 `Component.onCompleted` 中：

```qml
// 先跳过动画，直接设置正确的值
_skipRemainingTimeAnimation = true
_displayRemainingSec = _remainingSec
// 下一帧恢复动画
Qt.callLater(function() {
    _skipRemainingTimeAnimation = false
})
```

### Step 4：确保 Timer 在后台持续运行

检查 `elapsedTimer` 的 `running` 条件，确保在处理状态下始终运行。

## 风险评估

- **低风险**：修改仅影响显示逻辑，不影响核心功能
- **兼容性**：不影响现有功能
- **测试点**：
  1. 切换会话后返回，剩余时间应直接显示正确值
  2. 正常处理时，剩余时间应平滑变化
  3. 超时情况下，应正确显示超时时间

## 预期效果

- 切换会话后返回，剩余时间直接显示正确的值
- 不再出现"从0开始变化"的现象
- 正常处理时，动画效果保持不变
