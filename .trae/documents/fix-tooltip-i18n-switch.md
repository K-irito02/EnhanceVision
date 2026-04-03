# 修复：AI推理面板 CPU/GPU 卡片 Tooltip 语言切换不生效

## 问题分析

### 现象
在 AI 推理模式右侧控制面板中，鼠标悬停在 CPU/GPU 推理卡片上显示 Tooltip，切换中英文语言后，Tooltip 文本仍显示中文，不跟随语言切换。

### 根因定位

**文件**: [AIParamsPanel.qml](qml/components/AIParamsPanel.qml)

**问题代码（CPU Tooltip，第 366-373 行）**:
```qml
Tooltip {
    id: _cpuTooltip
    property string _lang: Theme.language
    text: {
        var _ = _cpuTooltip._lang   // 依赖 Theme.language 触发重估
        return qsTr("CPU 推理模式：使用处理器进行 AI 计算。\n...")
    }
}
```

**GPU Tooltip（第 504-511 行）** 同样模式。

**根因链路**:

1. `qsTr()` 被包裹在 JavaScript 块 `{ var _ = ...; return qsTr(...) }` 中，而非直接作为属性绑定
2. 当 `Theme.language` 变更时，虽然 `_lang` 属性变化触发了 `text` 绑定的重新评估
3. 但 `QQmlEngine::retranslate()` 对「JS 块内的 `qsTr()`」的检测和刷新不可靠——这类复杂绑定不会被识别为纯翻译绑定
4. 更关键的是信号时序问题：
   - `SettingsController.setLanguage()` → 先 emit `languageChanged()` → QML 立即开始重估绑定
   - 此时 `Application.onLanguageChanged()` 尚未执行 `switchTranslator()` + `engine()->retranslate()`
   - 所以 `qsTr()` 重估时翻译器仍是旧语言 → 返回旧文本
5. 语言变更处理器（第 800-814 行）只处理了 `currentModelInfo` 和 `_paramsRepeater.model` 的刷新，**完全没有处理 Tooltip**

## 修复方案

### 修改文件
- [AIParamsPanel.qml](qml/components/AIParamsPanel.qml) — 唯一需要修改的文件

### 修改内容

#### 1. 在语言切换回调中关闭已打开的 Tooltip（核心修复）

在已有的两个 `onLanguageChanged` 处理器中，添加关闭 Tooltip 的逻辑：

```qml
// SettingsController.languageChanged (第 802 行)
Connections {
    target: SettingsController
    function onLanguageChanged() {
        console.log("[AIParamsPanel] SettingsController.languageChanged signal received")
        if (registry && modelId !== "") currentModelInfo = registry.getModelInfoMap(modelId)
        _paramsRepeater.model = _paramsRepeater.model
        _cpuTooltip.close()   // ← 新增
        _gpuTooltip.close()   // ← 新增
    }
}

// Theme.languageChanged (第 810 行)
Connections {
    target: Theme
    function onLanguageChanged() {
        console.log("[AIParamsPanel] Theme.languageChanged signal received, language:", Theme.language)
        _paramsRepeater.model = _paramsRepeater.model
        _cpuTooltip.close()   // ← 新增
        _gpuTooltip.close()   // ← 新增
    }
}
```

**效果**：语言切换时立即关闭当前打开的 Tooltip；用户再次悬停时，Tooltip 以新语言重新展示。

#### 2. （可选增强）简化 Tooltip 文本绑定以提高可靠性

将当前的 JS 块绑定改为更简洁的形式，提升 `retranslate()` 的识别率：

**修改前**:
```qml
text: {
    var _ = _cpuTooltip._lang
    return qsTr("CPU 推理模式：使用处理器进行 AI 计算。\n优点：兼容所有硬件，稳定性高，内存占用可控。\n缺点：处理速度较慢，大图和视频耗时较长。")
}
```

**修改后**:
```qml
text: qsTr("CPU 推理模式：使用处理器进行 AI 计算。\n优点：兼容所有硬件，稳定性高，内存占用可控。\n缺点：处理速度较慢，大图和视频耗时较长。")
```

同时移除不再需要的 `property string _lang: Theme.language`（CPU 和 GPU Tooltip 各一个）。

> **注意**：去掉 `_lang` 依赖后，`qsTr()` 将完全依赖 `engine()->retranslate()` 来触发更新。配合步骤 1 的 close 操作，用户再次 hover 时一定会拿到最新翻译。

## 影响范围评估

| 影响项 | 说明 |
|--------|------|
| 影响文件 | 仅 `AIParamsPanel.qml` |
| 影响功能 | CPU/GPU 卡片的悬停 Tooltip |
| 风险等级 | 低 —— 仅添加 close 调用 + 简化绑定表达式 |
| 回归风险 | 无 —— 不改变任何现有交互逻辑和视觉效果 |

## 验证方法

1. 构建并启动应用
2. 进入 AI 推理模式，打开右侧控制面板
3. 鼠标悬停在 CPU 卡片上 → 确认 Tooltip 显示中文
4. 切换语言为英文 → Tooltip 应关闭（如果当时处于打开状态）
5. 再次悬停 CPU 卡片 → 确认 Tooltip 显示英文
6. 同样测试 GPU 卡片
7. 切换回中文 → 再次验证两卡片 Tooltip 显示中文
