# 修复计划：AI控制面板模型参数间歇性不显示 + 无法滚动

## 问题描述

1. **间歇性 Bug**：有时打开应用程序后，AI 控制面板的「模型参数」区域中参数（如 `face_enhance`、`outscale`）不显示
2. **滚动失效**：模型参数设置区域无法通过鼠标滚轮滚动

## 根因分析

### Bug 1：参数间歇性不显示 — 竞态条件

**调用链路：**
```
用户选择模型 → modelId 变化 → onModelIdChanged → _updateModelInfo()
→ registry.getModelInfoMap(modelId) → currentModelInfo → Repeater.model 显示参数
```

**根因点：**

| 位置 | 问题 | 影响 |
|------|------|------|
| [AIParamsPanel.qml:25](qml/components/AIParamsPanel.qml#L25) | `registry` 惰性绑定 `processingController ? processingController.modelRegistry : null` | 组件创建时若 C++ 端未完成初始化，返回 `null` |
| [AIParamsPanel.qml:64-81](qml/components/AIParamsPanel.qml#L64-L81) | `_updateModelInfo()` 无空值防御和重试机制 | registry 为 null 时静默失败，之后不会自动恢复 |
| [AIParamsPanel.qml:76](qml/components/AIParamsPanel.qml#L76) | Timer 仅 100ms 延迟，无重试 | 慢系统上 registry 可能仍未就绪 |
| [AIParamsPanel.qml:431-448](qml/components/AIParamsPanel.qml#L431-L448) | Repeater.model 首次求值为空后不再刷新 | QML 绑定惰性求值，引用不变则不重新计算 |

### Bug 2：无法鼠标滚动 — ScrollView 内容高度 + 事件拦截

| 位置 | 问题 | 影响 |
|------|------|------|
| [ControlPanel.qml:89-97](qml/components/ControlPanel.qml#L89-L97) | `globalClickHandler` MouseArea (`z:999`) 在编辑模式下拦截所有鼠标事件 | 滚轮事件被吞掉 |
| [AIParamsPanel.qml:37](qml/components/AIParamsPanel.qml#L37) | `Layout.minimumHeight: 300` 固定最小高度 | 干扰 ScrollView 对内容实际高度的判断 |
| [ControlPanel.qml:321-336](qml/components/ControlPanel.qml#L321-L336) | ScrollView 内嵌 ColumnLayout，implicitHeight 传递可能异常 | ScrollBar 不出现或内容不可滚 |

---

## 修复方案

### 修复步骤 1：增强 _updateModelInfo() 的防御性和重试机制

**文件**: [AIParamsPanel.qml](qml/components/AIParamsPanel.qml)

**修改 `_updateModelInfo()` 函数（第64-81行）：**
- 增加 registry 空值检查，若为 null 则启动延迟重试 Timer
- 新增 `_modelLoadRetryTimer`（interval: 200ms，最大重试 5 次）
- 当 registry 从 null 变为有效值时（onRegistryChanged），如果当前有 modelId 则立即重新获取

```javascript
// 新增属性
property int _modelLoadRetryCount: 0

// 新增重试定时器
Timer {
    id: _modelLoadRetryTimer
    interval: 200
    onTriggered: {
        if (_modelLoadRetryCount < 5) {
            _modelLoadRetryCount++
            _updateModelInfo()
        } else {
            _modelLoadRetryCount = 0
        }
    }
}

// 修改 _updateModelInfo
function _updateModelInfo() {
    if (registry && modelId !== "") {
        // registry 有效，正常流程
        _isModelSwitching = true
        currentModelInfo = registry.getModelInfoMap(modelId)
        // ... 原有逻辑
        _modelLoadRetryCount = 0  // 重置重试计数
    } else if (modelId !== "" && !registry) {
        // registry 尚未就绪，延迟重试
        _modelLoadRetryTimer.restart()
    } else {
        currentModelInfo = null
    }
}
```

### 修复步骤 2：增强 onRegistryChanged 处理

**文件**: [AIParamsPanel.qml](qml/components/AIParamsPanel.qml)

**修改第61行的 `onRegistryChanged`：**
- 当 registry 从无效变为有效时，如果已有 modelId，主动触发一次模型信息更新

```javascript
onRegistryChanged: {
    if (registry && modelId !== "") {
        _updateModelInfo()
    }
}
```

### 修复步骤 3：移除 AIParamsPanel 的固定 Layout.minimumHeight

**文件**: [AIParamsPanel.qml](qml/components/AIParamsPanel.qml)

**修改第37行：**
- 删除或注释掉 `Layout.minimumHeight: 300`
- 改用 `Layout.minimumHeight: 0` 让 ScrollView 根据实际内容计算高度
- 这确保当内容较少时 ScrollView 也能正确判断是否需要滚动

### 修复步骤 4：修复 globalClickHandler 的滚轮事件透传

**文件**: [ControlPanel.qml](qml/components/ControlPanel.qml)

**修改第89-97行的 globalClickHandler MouseArea：**
- 增加 `onWheel` 事件处理，将滚轮事件传递给下层组件
- 或者将 `z` 值降低 / 调整 enabled 条件范围

```qml
MouseArea {
    id: globalClickHandler
    anchors.fill: parent
    enabled: root.isAnyEditing
    z: 999
    onClicked: { root.isAnyEditing = false }
    onWheel: function(wheel) {
        // 透传滚轮事件，不拦截
        wheel.accepted = false
    }
}
```

### 修复步骤 5：确保 ScrollView 内容正确报告高度

**文件**: [ControlPanel.qml](qml/components/ControlPanel.qml)

**确认 aiParamsScrollView 配置（第321-336行）：**
- 验证 `clip: true` 正确设置
- 确保 AIParamsPanel 的 width 绑定到 `aiParamsScrollView.availableWidth - 4` 是正确的

---

## 影响评估

| 变更项 | 影响范围 | 风险等级 |
|--------|----------|----------|
| 步骤1-2：重试机制 | 仅影响 AIParamsPanel 模型加载时序 | 低 — 纯新增防御逻辑 |
| 步骤3：移除 minimumHeight | AIParamsPanel 在 ScrollView 中的布局 | 低 — 可能改变默认视觉大小 |
| 步骤4：wheel 透传 | ControlPanel 编辑模式下的鼠标行为 | 低 — 仅影响编辑模式 |
| 步骤5：ScrollView 配置验证 | AI 参数区域滚动行为 | 无 — 确认性质 |

## 验证方法

1. 多次快速打开/关闭应用，观察模型参数是否每次都正确显示
2. 选择不同模型，切换后确认参数实时更新
3. 在模型参数区域使用鼠标滚轮测试滚动功能
4. 进入编辑模式后测试滚轮是否仍然可用
5. 检查应用日志中无相关警告或错误
