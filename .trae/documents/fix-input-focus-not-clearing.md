# 修复计划：QML 输入框焦点不消失问题

## 问题描述

用户反馈两类输入框在获得鼠标焦点后，点击主界面其他区域时**焦点不会自动消失**：
1. **会话标签重命名输入框** — `SessionList.qml` 第425-470行的 `TextField`
2. **Shader参数调节输入框（14个）** — `ParamSlider.qml` 第122-168行的 `TextInput`

## 根因分析

### 现有架构
- `App.qml` 根组件是 `FocusScope`，已有 `focusCatcher`(不可见Item, `focus:true`) 和 `clearAllFocus()` 函数
- `App.qml` 第65-69行有一个 `TapHandler`，意图在点击时调用 `focusCatcher.forceActiveFocus()` 清除焦点

### 问题根因
**TapHandler 的位置错误**：当前 TapHandler 位于第65行，在背景 Rectangle(第84行)和主布局 ColumnLayout(第91行)**之前**。由于 Qt Quick 的渲染顺序和事件传递机制，后声明的子元素 z-order 更高，会覆盖先声明的元素。因此：

```
声明顺序（从上到下）= z-order（从低到高）
第65行: TapHandler          ← z-order 最低，被遮挡
第84行: backgroundRect     ← 覆盖 TapHandler
第91行: ColumnLayout(主布局) ← 覆盖 TapHandler
...其他所有UI元素...
```

当用户点击输入框外部区域时：
1. 点击事件被 `backgroundRect` 或主布局内的子元素接收
2. 这些元素不是可聚焦的输入控件，不会触发焦点转移逻辑
3. **TapHandler 收不到事件**（被遮挡），因此 `clearAllFocus()` 从未被调用
4. 输入框保持 `activeFocus = true`

### 在线研究结论

来自 Qt Forum、Qt 官方文档及开发者社区的共识方案：
- **Qt Forum #154429**: TextField 的 `activeFocusOnPress: true`（默认值）导致焦点行为异常，推荐使用 TapHandler + 手动焦点管理
- **Qt Quick Keyboard Focus 文档**: 推荐使用 FocusScope 包裹复杂组件，并在窗口级别管理焦点
- **最佳实践**: 在窗口/根组件层级放置全局 TapHandler/MouseArea 捕获"空白区域"点击来清除输入焦点

## 解决方案：三层防护策略

### 第一层（核心修复）：修正 App.qml 全局 TapHandler 位置

**改动文件**: [App.qml](qml/App.qml)

**具体操作**:
1. 将第65-69行的 `TapHandler` 从当前位置**移到文件末尾**（在 `OffscreenShaderRenderer` 之后）
2. 这样 TapHandler 拥有最高的 z-order，能接收到所有未被子交互元素消费的点击事件
3. TapHandler 的特性：与 MouseArea 不同，**TapHandler 默认不独占/阻止事件向下传播**，子元素的 Button、MouseArea 等仍正常工作

**修改前代码** (第55-69行):
```qml
function clearAllFocus() {
    focusCatcher.forceActiveFocus()
}

Item {
    id: focusCatcher
    focus: true
    visible: false
}

TapHandler {                          // ← 当前位置：被后续UI元素覆盖
    onTapped: function(eventPoint, button) {
        focusCatcher.forceActiveFocus()
    }
}
```

**修改后代码**:
```qml
function clearAllFocus() {
    focusCatcher.forceActiveFocus()
}

Item {
    id: focusCatcher
    focus: true
    visible: false
}

// ... 所有现有 UI 元素保持不变 ...

// ========== 全局焦点清除器（置于最上层）==========
TapHandler {
    onTapped: function(eventPoint, button) {
        root.clearAllFocus()
    }
}
```

> **为什么有效**: 移到最后后，TapHandler 的 z-order 最高。当用户点击非交互区域（背景、空白处等），事件冒泡到 TapHandler 并触发 `clearAllFocus()`。当点击 Button/MouseArea/Slider 等交互元素时，这些元素消费了点击事件，TapHandler 不触发——这正是期望的行为。

### 第二层：增强 ParamSlider.qml 的 TextInput 焦点处理

**改动文件**: [ParamSlider.qml](qml/controls/ParamSlider.qml)

**具体操作**:
1. 为 `TextInput`(第122行)添加 `Keys.onTabPressed` 处理器，支持 Tab 键转移焦点
2. 验证 `onEditingFinished`(第148行) 的 `focus = false` 能正确触发
3. 确保 TextInput 不设置 `activeFocusOnPress: false`（保持默认 true）

**新增代码** (在第167行 `Keys.onEscapePressed` 之后):
```qml
Keys.onTabPressed: {
    focus = false
    event.accepted = true  // 阻止默认 Tab 导航（避免跳到意外位置）
}
```

### 第三层：验证 SessionList.qml TextField 行为

**改动文件**: [SessionList.qml](qml/components/SessionList.qml)

**具体操作**:
1. 验证现有的 `onActiveFocusChanged`(第447行)在外部点击时能正确触发
2. 如果 TapHandler 修复后仍有问题，为 TextField 添加显式的 `Keys.onTabPressed` 处理

> 注：此层主要是验证性工作，第一层修复后该 TextField 应自动获得正确的焦点清除能力。

## 影响范围评估

| 影响区域 | 影响 | 风险 |
|---------|------|------|
| App.qml TapHandler 位置 | 仅移动代码位置，逻辑不变 | 极低 |
| ParamSlider.qml 新增 Tab 处理 | 仅增加 Tab 键支持 | 无 |
| SessionList.qml | 可能无需修改 | 无 |
| 其他输入框（ShaderParamsPanel 内其他 TextField） | 自动受益于全局 TapHandler | 正面影响 |
| 按钮/Slider/其他控件点击 | TapHandler 不干扰（不独占事件） | 无 |

## 实施步骤

1. **修改 App.qml**：将 TapHandler 移至文件末尾
2. **修改 ParamSlider.qml**：为 TextInput 添加 Keys.onTabPressed
3. **构建验证**：使用 qt-build-and-fix 技能构建并运行项目
4. **功能测试**：
   - 点击会话标签重命名输入框 → 获得焦点 → 点击外部 → 焦点应消失
   - 点击 Shader 参数输入框 → 输入数值 → 点击外部 → 焦点应消失
   - Tab 键在输入框间切换应正常
   - Escape 键取消编辑应正常
   - 回车键确认输入应正常
   - 所有按钮、Slider 等控件点击不受影响
5. **日志检查**：确认无警告或错误
