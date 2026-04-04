# AI 模型切换参数失效问题修复计划

## 问题分析

### 问题现象
用户在 AI 推理模式右边控制面板下，不停在多个模型间进行切换时，模型设置区域虽然能够自动变化，但是切换次数过多后，就不会再变化了。

### 根本原因分析

通过深入分析 `AIParamsPanel.qml`、`AIModelPanel.qml` 和 `ControlPanel.qml`，发现以下问题：

#### 1. 属性绑定断开问题
```qml
// AIParamsPanel.qml 第 37 行
property var currentModelInfo: (registry && modelId !== "") ? registry.getModelInfoMap(modelId) : null
```
- 这是一个**计算属性**，依赖于 `registry` 和 `modelId`
- 当频繁切换模型时，QML 的属性绑定可能会断开
- 特别是在 JavaScript 表达式中调用 C++ 方法时，绑定不够稳定

#### 2. Repeater 模型更新问题
```qml
// AIParamsPanel.qml 第 642-662 行
Repeater {
    id: _paramsRepeater
    model: {
        var _ver = root._paramsModelVersion
        if (!currentModelInfo) return []
        // ...复杂的 JavaScript 表达式
    }
}
```
- Repeater 的 model 绑定使用了复杂的 JavaScript 表达式
- 虽然 `_paramsModelVersion` 用于强制刷新，但机制不够可靠
- 频繁更新可能导致 Repeater 内部状态混乱

#### 3. 状态重置问题
```qml
// AIParamsPanel.qml 第 59-72 行
onCurrentModelInfoChanged: function(info) {
    if (info) {
        _isModelSwitching = true
        // ... 重置所有状态
        modelParams = {}  // 这里会清空用户修改的参数
        // ...
    }
}
```
- 每次模型切换都会重置 `modelParams`
- 频繁切换可能导致状态不一致

#### 4. 信号连接问题
```qml
// ControlPanel.qml 第 306-310 行
onModelSelected: function(modelId, category) {
    root.aiSelectedModelId = modelId
    root.aiSelectedCategory = category
    aiParamsPanel.modelId = modelId
}
```
- 信号处理中直接赋值，可能破坏属性绑定

### 用户方案评估

用户提出的方案：**硬编码模型和对应的模型设置参数，放在另一个 QML 文件中**

**优点**：
- 配置与 UI 分离，职责单一
- 避免动态计算带来的问题
- 易于维护和扩展

**缺点**：
- 硬编码会导致配置与 `models.json` 不同步
- 失去动态加载模型的灵活性
- 需要手动维护两份配置

## 推荐方案：混合架构

结合用户建议和最佳实践，采用以下方案：

### 方案架构

```
┌─────────────────────────────────────────────────────────────┐
│                    AIModelConfigStore.qml                    │
│  (单例 - 模型配置缓存与状态管理)                               │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ 模型配置缓存  │  │ 参数状态缓存  │  │ 绑定安全层   │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
         │                    │                    │
         ▼                    ▼                    ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│  AIModelPanel   │  │  AIParamsPanel  │  │  ControlPanel   │
│  (模型选择)      │  │  (参数配置)      │  │  (整合控制)      │
└─────────────────┘  └─────────────────┘  └─────────────────┘
```

### 核心改进点

#### 1. 创建 AIModelConfigStore.qml（单例）
- 缓存所有模型配置信息
- 管理每个模型的参数状态
- 提供稳定的属性绑定接口
- 实现参数持久化（用户修改的参数在模型切换时保留）

#### 2. 修复属性绑定
- 使用 `Qt.binding()` 确保绑定稳定
- 避免在信号处理中直接赋值破坏绑定

#### 3. 优化 Repeater 更新机制
- 使用 `ListModel` 替代 JavaScript 数组
- 使用 `Loader` 管理 Repeater 的生命周期

#### 4. 参数状态缓存
- 缓存每个模型的用户修改参数
- 模型切换时恢复之前的参数设置

## 实施步骤

### 第一步：创建 AIModelConfigStore.qml
创建 `qml/stores/AIModelConfigStore.qml` 单例组件：
- 从 ModelRegistry 加载并缓存模型配置
- 管理每个模型的参数状态
- 提供稳定的属性绑定接口

### 第二步：重构 AIParamsPanel.qml
修改 `qml/components/AIParamsPanel.qml`：
- 从 AIModelConfigStore 获取模型信息
- 使用 Qt.binding() 确保属性绑定稳定
- 优化 Repeater 的 model 绑定

### 第三步：修改 ControlPanel.qml
修改 `qml/components/ControlPanel.qml`：
- 使用 AIModelConfigStore 管理状态
- 优化模型选择信号处理

### 第四步：修改 AIModelPanel.qml
修改 `qml/components/AIModelPanel.qml`：
- 从 AIModelConfigStore 获取模型列表
- 优化类别和模型选择逻辑

### 第五步：测试验证
- 构建并运行项目
- 测试频繁模型切换场景
- 验证参数设置是否正常保持

## 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `qml/stores/AIModelConfigStore.qml` | 新建 | 模型配置状态管理单例 |
| `qml/components/AIParamsPanel.qml` | 修改 | 重构属性绑定和状态管理 |
| `qml/components/AIModelPanel.qml` | 修改 | 优化模型选择逻辑 |
| `qml/components/ControlPanel.qml` | 修改 | 使用新的状态管理 |

## 技术细节

### AIModelConfigStore.qml 核心设计

```qml
// qml/stores/AIModelConfigStore.qml
pragma Singleton
import QtQuick 2.15
import EnhanceVision.Controllers

QtObject {
    id: store
    
    // 当前选中的模型 ID
    property string currentModelId: ""
    
    // 当前模型信息（稳定绑定）
    readonly property var currentModelInfo: {
        if (currentModelId === "" || !registry) return null
        return _getModelInfoCached(currentModelId)
    }
    
    // 模型参数缓存（每个模型独立保存）
    property var _paramsCache: ({})
    
    // 当前模型参数
    property var currentModelParams: {
        if (!currentModelId) return {}
        return _paramsCache[currentModelId] || _getDefaultParams(currentModelId)
    }
    
    // 模型信息缓存
    property var _modelInfoCache: ({})
    
    // 获取模型信息（带缓存）
    function _getModelInfoCached(modelId) {
        if (!_modelInfoCache[modelId] && registry) {
            _modelInfoCache[modelId] = registry.getModelInfoMap(modelId)
        }
        return _modelInfoCache[modelId] || null
    }
    
    // 获取默认参数
    function _getDefaultParams(modelId) {
        var info = _getModelInfoCached(modelId)
        if (!info || !info.supportedParams) return {}
        var defaults = {}
        var params = info.supportedParams
        var keys = Object.keys(params)
        for (var i = 0; i < keys.length; i++) {
            var key = keys[i]
            if (params[key]["default"] !== undefined) {
                defaults[key] = params[key]["default"]
            }
        }
        return defaults
    }
    
    // 切换模型
    function selectModel(modelId) {
        if (currentModelId === modelId) return
        currentModelId = modelId
    }
    
    // 更新参数
    function updateParam(paramKey, value) {
        var params = Object.assign({}, currentModelParams)
        params[paramKey] = value
        _paramsCache[currentModelId] = params
        currentModelParamsChanged()
    }
    
    // 重置当前模型参数
    function resetCurrentParams() {
        _paramsCache[currentModelId] = _getDefaultParams(currentModelId)
        currentModelParamsChanged()
    }
}
```

### 属性绑定修复方案

```qml
// AIParamsPanel.qml 修改后的关键部分

// 使用 Qt.binding() 确保绑定稳定
readonly property var currentModelInfo: Qt.binding(function() {
    return AIModelConfigStore.currentModelInfo
})

// 使用 ListModel 替代 JavaScript 数组
ListModel {
    id: paramsListModel
}

// 在模型信息变化时更新 ListModel
Connections {
    target: AIModelConfigStore
    function onCurrentModelInfoChanged() {
        paramsListModel.clear()
        var info = AIModelConfigStore.currentModelInfo
        if (info && info.supportedParams) {
            var keys = Object.keys(info.supportedParams)
            for (var i = 0; i < keys.length; i++) {
                var key = keys[i]
                var param = info.supportedParams[key]
                paramsListModel.append({
                    key: key,
                    type: param.type || "float",
                    defaultValue: param["default"],
                    min: param.min,
                    max: param.max,
                    step: param.step,
                    label: param.label || key,
                    description: param.description || ""
                })
            }
        }
    }
}
```

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 新单例引入复杂度 | 中 | 保持接口简洁，充分注释 |
| 状态同步问题 | 低 | 使用 Qt.binding() 和信号机制 |
| 性能影响 | 低 | 使用缓存机制，避免重复计算 |

## 验收标准

1. 频繁切换模型（10次以上）后，参数设置区域仍能正常更新
2. 切换回之前选过的模型时，用户修改的参数能够保留
3. 构建无错误，运行无警告
4. 不影响现有功能

## 用户确认

- **方案选择**：采用混合架构方案（推荐）
- **参数缓存**：是，保留参数（推荐）
- **补充要求**：每个模型之间的参数都是独立的，不要相互影响

## 预计工作量

- 创建 AIModelConfigStore.qml：约 150 行代码
- 修改 AIParamsPanel.qml：约 50 行修改
- 修改 AIModelPanel.qml：约 30 行修改
- 修改 ControlPanel.qml：约 20 行修改
- 测试验证：构建运行测试
