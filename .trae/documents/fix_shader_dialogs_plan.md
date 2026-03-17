# 修复 Shader 模式控制面板对话框问题 - 实施计划

## 问题分析

### 问题 1：关闭按钮不工作
- **原因**：ShaderParamsPanel 中的对话框缺少 `updateExcludeRegions()` 函数调用，没有将关闭按钮区域排除在 SubWindowHelper 的拖拽检测之外
- **对比**：MediaViewerWindow.qml 正确实现了该功能

### 问题 2：窗口布局不美观（有大量空白区域）
- **原因**：窗口高度计算方式不正确，Binding 中的计算逻辑有问题
- **解决思路**：修复高度计算，确保窗口高度自适应内容

## 任务列表

### [ ] 任务 1：修复 DialogTitleBar 组件的层级问题
- **优先级**：P0
- **描述**：确保 DialogTitleBar 中的关闭按钮层级正确，不会被其他元素遮挡
- **成功标准**：关闭按钮可以正常响应悬停和点击事件
- **测试要求**：
  - `human-judgement` TR-1.1：鼠标悬停在关闭按钮上时，按钮有视觉反馈
  - `human-judgement` TR-1.2：点击关闭按钮可以正常关闭窗口

### [ ] 任务 2：为所有对话框添加 updateExcludeRegions 函数
- **优先级**：P0
- **描述**：为 ShaderParamsPanel 中的每个对话框（5个）添加排除区域功能
- **成功标准**：SubWindowHelper 正确排除关闭按钮区域
- **测试要求**：
  - `human-judgement` TR-2.1：关闭按钮可以正常点击，不会被拖拽区域拦截
- **涉及的对话框**：
  - deleteCategoryDialog
  - addCategoryDialog
  - renameCategoryDialog
  - renamePresetDialog
  - savePresetDialog

### [ ] 任务 3：修复窗口高度计算问题
- **优先级**：P1
- **描述**：修复所有对话框的高度 Binding，确保窗口高度根据内容自适应
- **成功标准**：对话框高度合适，没有过多的空白区域
- **测试要求**：
  - `human-judgement` TR-3.1：对话框布局美观，内容完整显示
  - `human-judgement` TR-3.2：窗口底部与按钮的距离合适

## 实施步骤

1. 修改 DialogTitleBar.qml，确保关闭按钮层级正确
2. 为每个对话框添加 updateExcludeRegions 函数
3. 在对话框的 onWidthChanged、onHeightChanged、onVisibleChanged 中调用该函数
4. 修复每个对话框的高度 Binding
