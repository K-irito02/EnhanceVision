# 修复计划：AI 推理面板模型参数间歇性不显示 + 滚轮支持（根治方案）

## 问题概述

### 问题 1：间歇性参数不显示
**现象**：切换模型后，"模型参数"标题显示但下方的具体参数控件（Slider/Switch 等）偶尔不显示。

### 问题 2：模型设置区域滚轮滚动不支持
**现象**：鼠标在 "模型设置" 区域上滚动时，无法通过滚轮上下滚动查看更多参数。

---

## 根因分析

### 问题 1 根因：命令式赋值导致竞态条件

**当前架构缺陷**：
```
currentModelInfo 声明为 null（第 37 行）
       ↓
modelId 变化 → onModelIdChanged → _updateModelInfo() [命令式赋值]
registry 变化 → onRegistryChanged → _updateModelInfo() [命令式赋值]
registry 为 null → _modelLoadRetryTimer [轮询重试，最多 1 秒]
```

这是一个 **命令式（imperative）数据流模式**，存在以下根本性问题：

| 缺陷 | 说明 |
|------|------|
| **时序依赖** | `modelId` 和 `registry` 的就绪顺序不确定，必须手动协调 |
| **轮询重试不可靠** | `_modelLoadRetryTimer` 仅 5×200ms=1 秒，超时即永久失败 |
| **绑定断裂** | `_paramsRepeater.model` 依赖 `currentModelInfo`，但 `currentModelInfo` 是命令式赋值，QML 绑定引擎无法保证依赖追踪完整性 |
| **状态碎片化** | 需要维护 `_isModelSwitching`、`_modelLoadRetryCount`、`_paramsModelVersion` 等多个同步状态 |

**根治思路**：将 `currentModelInfo` 从**命令式赋值**改为**声明式绑定（declarative binding）**，让 QML 绑接引擎自动管理依赖关系和求值时序。

### 问题 2 根因：全局 MouseArea 拦截滚轮事件

**布局层级**：
```
ControlPanel (Rectangle)
  ├── globalClickHandler (MouseArea, z=999, enabled: isAnyEditing)
  │   ├── onClicked: 退出编辑模式 ✓（正确用途）
  │   └── onWheel: wheel.accepted = false ✗（多余且有害）
  └── ColumnLayout → StackLayout → aiContentLayout
       └── ScrollView (aiParamsScrollView) ← 已有正确的 ScrollView 配置
            └── AIParamsPanel
```

`globalClickHandler` 的唯一合法用途是**点击退出编辑模式**。`onWheel` 拦截是多余的——它阻止了 `aiParamsScrollView`（已正确配置 ScrollBar.vertical.policy: AsNeeded）接收滚轮事件。

**根治思路**：删除 `globalClickHandler` 的 `onWheel` 处理器。滚轮事件自然传递到下层 ScrollView。

---

## 根治方案

### 方案 1：声明式绑定替代命令式赋值（问题 1）

#### 核心变更

**Before（命令式 — 有竞态）**：
```qml
// 第 37 行
property var currentModelInfo: null

// 第 78 行
onModelIdChanged: _updateModelInfo()

// 第 73-77 行
onRegistryChanged: {
    if (registry && modelId !== "") _updateModelInfo()
}

// 第 80-100 行
function _updateModelInfo() {
    if (registry && modelId !== "") {
        _isModelSwitching = true
        currentModelInfo = registry.getModelInfoMap(modelId)  // ← 手动赋值
        // ... 副作用 ...
    } else if (modelId !== "" && !registry) {
        _modelLoadRetryTimer.restart()  // ← 轮询 hack
    }
}
```

**After（声明式 — 零竞态）**：
```qml
// 第 37 行 — 改为绑定表达式
property var currentModelInfo: (registry && modelId !== "")
    ? registry.getModelInfoMap(modelId) : null

// 删除 onModelIdChanged: _updateModelInfo()  — 不再需要
// 删除 onRegistryChanged                 — 不再需要
// 删除 _modelLoadRetryTimer               — 不再需要
// 删除 _modelLoadRetryCount               — 不再需要

// 副作用移至 onCurrentModelInfoChanged
onCurrentModelInfoChanged: function(info) {
    if (info) {
        _isModelSwitching = true
        autoTileMode = true
        tileSize = 0
        computedAutoTileSize = 0
        _autoParams = {}
        _pendingParams = {}
        modelParams = {}
        _paramsModelVersion++
        _recomputeAutoParamsTimer.restart()
        _isModelSwitching = false
    }
}
```

#### 为什么这是彻底的修复

| 维度 | 命令式（旧） | 声明式（新） |
|------|-------------|-------------|
| 数据流 | 回调链驱动 | 绑定引擎驱动 |
| 时序安全 | 依赖调用顺序 | 引擎自动拓扑排序 |
| 重试机制 | 需要 Timer 轮询 | 完全不需要 |
| 状态同步 | 多个标志位手动协调 | 单一属性自动传播 |
| 失败恢复 | 可能永久空白 | registry 就绪后立即显示 |

QML 声明式绑定的保证：当 `registry` 或 `modelId` 任一变化时，绑定引擎自动重新求值 `currentModelInfo`。无论两者变化顺序如何、间隔多久，最终结果都正确。

#### 需要保留的代码

- `_recomputeAutoParamsTimer`（用于延迟计算自动参数）
- `_isModelSwitching`（用于动画过渡效果）
- `_paramsModelVersion`（用于强制 Repeater 刷新）
- `_autoParams`、`_pendingParams` 等运行时状态
- `getParams()`、`hasParamModifications()` 等业务函数
- 语言切换 Connections（仍需刷新 Repeater 显示翻译文本）

#### 需要删除的代码

- `_modelLoadRetryTimer`（整个 Timer 对象，第 53-64 行）
- `_modelLoadRetryCount`（第 43 行）
- `onModelIdChanged: _updateModelInfo()`（第 78 行）
- `onRegistryChanged`（第 73-77 行）
- `_updateModelInfo()` 函数中的数据获取逻辑（改为纯副作用处理）

### 方案 2：移除全局滚轮拦截（问题 2）

#### 核心变更

**文件**：[ControlPanel.qml](qml/components/ControlPanel.qml#L89-L100)

**Before**：
```qml
MouseArea {
    id: globalClickHandler
    anchors.fill: parent
    enabled: root.isAnyEditing
    z: 999
    onClicked: { root.isAnyEditing = false }
    onWheel: function(wheel) { wheel.accepted = false }  // ← 删除此行
}
```

**After**：
```qml
MouseArea {
    id: globalClickHandler
    anchors.fill: parent
    enabled: root.isAnyEditing
    z: 999
    onClicked: { root.isAnyEditing = false }
    // onWheel 已删除 — 滚轮事件自然穿透到下层 ScrollView
}
```

#### 为什么这是彻底的修复

`globalClickHandler` 的职责是**拦截点击以退出编辑模式**，它不需要处理滚轮事件。删除 `onWheel` 后：

1. 滚轮事件不再被 z:999 的 MouseArea 拦截
2. 事件自然传递到 `aiParamsScrollView`（已配置 `ScrollBar.vertical.policy: AsNeeded`）
3. ScrollView 内置的 Flickable 自动处理滚轮滚动
4. 无需额外的 WheelHandler 或事件转发逻辑

---

## 实施步骤

### 步骤 1：重构 AIParamsPanel.qml — 声明式绑定

1. **修改 `currentModelInfo` 属性声明**（第 37 行）
   - 从 `null` 改为绑定表达式 `(registry && modelId !== "") ? registry.getModelInfoMap(modelId) : null`

2. **删除 `_modelLoadRetryTimer`**（第 53-64 行）
   - 整个 Timer 对象删除

3. **删除 `_modelLoadRetryCount`**（第 43 行）

4. **重构 `_updateModelInfo()`**（第 80-100 行）
   - 改名为 `_resetModelSideEffects()` 或直接将逻辑移入 `onCurrentModelInfoChanged`
   - 只保留副作用（重置 autoTileMode、tileSize、_autoParams 等），删除数据获取逻辑
   - 确保 `_paramsModelVersion++` 在其中被调用

5. **删除 `onModelIdChanged: _updateModelInfo()`**（第 78 行）

6. **替换 `onRegistryChanged`**（第 73-77 行）
   - 改为 `onCurrentModelInfoChanged` 处理器
   - 或完全删除（如果副作用已在 onCurrentModelInfoChanged 中处理）

7. **验证语言切换 Connections**
   - `SettingsController.languageChanged` 中 `currentModelInfo = registry.getModelInfoMap(modelId)` 可删除（声明式绑定自动处理）
   - 保留 `_paramsRepeater.model = _paramsRepeater.model`（用于刷新翻译文本）
   - 保留 Tooltip close 调用

### 步骤 2：修改 ControlPanel.qml — 移除滚轮拦截

1. **删除 `globalClickHandler` 的 `onWheel`**（第 97-99 行）
   - 仅删除这两行，其他不变

### 步骤 3：构建验证

1. 使用 qt-build-and-fix 技能构建项目
2. 启动应用执行以下验证：
   - **问题 1 验证**：多次切换不同模型（Real-ESRGAN x4plus → AnimeVideo v3 → 反复切换），确认参数区始终正确显示
   - **问题 2 验证**：选择有多个参数的模型（如 RIFE v4.6 有 4 个参数），在参数区域使用鼠标滚轮确认可正常滚动
   - **回归验证**：CPU/GPU 切换、语言切换、分块大小调节等功能正常

---

## 影响范围评估

| 文件 | 修改类型 | 变更性质 |
|------|----------|----------|
| `qml/components/AIParamsPanel.qml` | 架构重构 | 命令式→声明式，删除约 20 行，修改约 10 行 |
| `qml/components/ControlPanel.qml` | 事件处理修复 | 删除 2 行 |

**不影响的功能**：
- Shader 模式的参数面板（独立 ScrollView）
- AI 模型选择面板（AIModelPanel）
- CPU/GPU 设备切换及 Tooltip
- 分块大小自动/手动模式
- 参数重置功能
- 所有现有信号连接和回调
