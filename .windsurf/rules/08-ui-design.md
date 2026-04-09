---
alwaysApply: false
globs: ['**/qml/styles/**', '**/qml/components/**', '**/qml/controls/**']
description: 'UI设计规范 - 主题系统、排版、交互状态'
trigger: glob
---
# UI 设计规范

## 主题系统

| 文件 | 职责 |
|------|------|
| `Theme.qml` | 主题单例，颜色/间距/圆角 |
| `Colors.qml` | 颜色定义（亮/暗） |
| `Typography.qml` | 字体定义 |

1. **禁止组件内硬编码颜色**：统一使用 `Theme.colors.xxx`
2. **亮/暗主题均需完整适配**
3. **主题切换使用 `Behavior` 动画**：`duration: 150`

## 图标规范

1. **统一使用 `Theme.icon()`**：返回 `qrc:/icons/xxx.svg`
2. **图标着色使用 `ColoredIcon`**：禁止直接 `Image`
3. **单套 SVG 资源**：动态着色适配主题

## 交互状态

| 状态 | 说明 |
|------|------|
| 默认 | 正常显示 |
| 悬停 | 鼠标悬停 |
| 按下 | 鼠标按下 |
| 禁用 | 不可用 |
| 焦点 | 键盘焦点 |
| 加载中 | 数据处理中 |

1. **关键操作必须提供明确反馈**
2. **空状态、异常状态必须有提示**
3. **视频切换时显示加载动画**

## 布局规范

1. **禁止固定宽度**：使用 `Layout.preferredWidth` + `Layout.minimumWidth`
2. **设置最小窗口**：`minimumWidth: 800`、`minimumHeight: 600`
3. **区块间距使用统一 spacing token**
4. **固定区与滚动区边界清晰**

## 按钮设计

1. **使用 IconButton 组件**：仅图标按钮
2. **按钮间距一致**：使用 `spacing: 2` 
3. **动态提示**：使用 `qsTr()` 国际化支持
4. **图标命名约定**：使用 kebab-case（例如 `view-source`、`view-result`）
5. **图标大小**：工具栏按钮使用 16px，主要操作使用 24px

## 图标指南

1. **Lucide 风格设计**：24x24 viewBox，stroke-width 2
2. **主题支持**：图标必须支持亮/暗主题切换
3. **单一 SVG 资源**：使用 `ColoredIcon` 动态着色
4. **图标放置**：存储在 `resources/icons/` 目录
5. **资源注册**：创建后添加到 `resources/qml.qrc`

## 可访问性

1. **键盘可达，焦点可见**
2. **文本与背景对比度 ≥ 4.5:1**
