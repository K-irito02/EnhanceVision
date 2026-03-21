# AI推理模式界面修复计划

## 问题分析

### 问题1: AI推理模式信息没有支持中英切换

**现状分析**：
- `AIModelPanel.qml` 和 `AIParamsPanel.qml` 中的文本已使用 `qsTr()` 包裹
- 但翻译文件 `app_zh_CN.ts` 和 `app_en_US.ts` 中缺少这两个组件的翻译条目
- 需要翻译的文本包括：
  - AIModelPanel: "AI 模型类别"、"请选择一个模型类别"
  - AIParamsPanel: "GPU 加速"、"可用"、"不可用"、"分块大小"、"自动"、"较小的分块占用更少显存，但处理更慢。0 = 自动。"、"模型参数"

**根本原因**：翻译文件未更新，缺少新添加组件的翻译条目。

### 问题2: 模型名称和描述相互重叠，且模型列表显示不完整

**现状分析**：
- `AIModelPanel.qml` 第297行设置了固定高度 `Layout.preferredHeight: 220`
- 超分辨率类别有15个模型，但面板高度仅220px，无法显示全部模型
- 模型列表项的布局可能导致名称与大小标签重叠

**根本原因**：
1. AIModelPanel 高度被固定，无法根据内容自适应
2. 模型列表项布局优化空间不足

---

## 实施步骤

### 步骤1: 修复 AIModelPanel.qml 布局问题

**文件**: `qml/components/AIModelPanel.qml`

**修改内容**:
1. 移除固定高度限制 `Layout.preferredHeight: 220`
2. 改为使用 `Layout.fillHeight: true` 让面板填充可用空间
3. 优化模型列表项布局，增加名称和大小标签之间的间距

### 步骤2: 更新中文翻译文件

**文件**: `resources/i18n/app_zh_CN.ts`

**添加内容**:
- AIModelPanel 组件的翻译条目
- AIParamsPanel 组件的翻译条目

### 步骤3: 更新英文翻译文件

**文件**: `resources/i18n/app_en_US.ts`

**添加内容**:
- AIModelPanel 组件的英文翻译
- AIParamsPanel 组件的英文翻译

---

## 详细修改清单

### 1. AIModelPanel.qml 布局修复

```qml
// 修改前 (第294-307行)
Components.AIModelPanel {
    id: aiModelPanel
    Layout.fillWidth: true
    Layout.preferredHeight: 220  // 移除此行

    selectedModelId: root.aiSelectedModelId
    selectedCategory: root.aiSelectedCategory
    ...
}

// 修改后
Components.AIModelPanel {
    id: aiModelPanel
    Layout.fillWidth: true
    Layout.fillHeight: true  // 改为填充高度

    selectedModelId: root.aiSelectedModelId
    selectedCategory: root.aiSelectedCategory
    ...
}
```

### 2. 翻译条目添加

**AIModelPanel 翻译**:
| 源文本 | 中文翻译 | 英文翻译 |
|--------|----------|----------|
| AI 模型类别 | AI 模型类别 | AI Model Categories |
| 请选择一个模型类别 | 请选择一个模型类别 | Please select a model category |

**AIParamsPanel 翻译**:
| 源文本 | 中文翻译 | 英文翻译 |
|--------|----------|----------|
| GPU 加速 | GPU 加速 | GPU Acceleration |
| 可用 | 可用 | Available |
| 不可用 | 不可用 | Unavailable |
| 分块大小 | 分块大小 | Tile Size |
| 自动 | 自动 | Auto |
| 较小的分块占用更少显存，但处理更慢。0 = 自动。 | 较小的分块占用更少显存，但处理更慢。0 = 自动。 | Smaller tiles use less VRAM but process slower. 0 = Auto. |
| 模型参数 | 模型参数 | Model Parameters |

---

## 验证步骤

1. 构建项目，确保无编译错误
2. 运行程序，切换到 AI 推理模式
3. 验证模型列表可以完整显示（超分辨率类别应显示全部15个模型）
4. 切换语言到英文，验证所有文本正确翻译
5. 切换语言回中文，验证显示正常
6. 检查模型名称和大小标签不再重叠
