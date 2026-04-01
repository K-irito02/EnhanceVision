# Dialog 对话框截断修复笔记

## 概述

修复设置页面"缓存管理"中清理确认对话框被主窗口底部截断的问题。采用响应式绑定（Reactive Binding）重构 Dialog 组件定位逻辑，彻底消除布局时序导致的定位错误。

**创建日期**: 2026-04-02
**作者**: AI Assistant

---

## 一、问题描述

### 现象

在设置页面的"缓存管理"区域，点击任意清理按钮后弹出的确认对话框被主窗口底部截断，无法完整显示对话框的全部内容（特别是内容较长的如"Shader - 视频"清理确认弹窗）。

### 根因分析

1. **命令式位置计算的时序缺陷**：原 `showDialog()` 函数在设置 `visible = true` 后立即读取 `dialogRect.implicitHeight` 计算居中位置，但此时 QML 布局引擎尚未完成子组件的布局计算，`implicitHeight` 仍为旧值或默认值
2. **无边界约束**：即使位置计算正确，当对话框内容高度超过父容器可用空间时，没有将 y 坐标限制在可视范围内
3. **无内容溢出保护**：长内容直接撑高对话框，无最大高度限制或内部滚动机制

---

## 二、解决方案

### 核心策略：响应式绑定替代命令式计算

**修改前（命令式）**：
```javascript
function showDialog(title, message, type, btnText) {
    // 设置属性...
    dialogPos = Qt.point(
        (parent.width - dialogRect.width) / 2,
        (parent.height - dialogRect.height) / 2  // 此时 height 可能未更新！
    )
    visible = true
    showAnimation.start()
}
```

**修改后（响应式绑定）**：
```qml
Rectangle {
    id: dialogRect
    x: __clampedCenterX + dragOffset.x  // 绑定表达式，自动跟随尺寸变化
    y: __clampedCenterY + dragOffset.y

    readonly property real __clampedCenterY: {
        var cy = (parent.height - implicitHeight) / 2
        return Math.max(0, Math.min(cy, parent.height - implicitHeight))
    }
}
```

### 三层防护机制

| 层级 | 措施 | 说明 |
|------|------|------|
| 第一层 | 响应式绑定定位 | x/y 由 QML Binding 自动计算，implicitHeight 变化时自动重新定位 |
| 第二层 | 边界约束 | `Math.max(0, min(centerY, parentHeight - dialogHeight))` 确保不越界 |
| 第三层 | 内容溢出处理 | 最大高度限制为父容器的 90%，超长内容通过 Flickable 滚动查看 |

---

## 三、变更文件

| 文件路径 | 修改类型 | 修改内容 |
|----------|----------|----------|
| `qml/components/Dialog.qml` | 重构 | 完全重写定位逻辑 |

### 具体变更点

1. **移除 `dialogPos` 属性和 Timer** — 不再有命令式位置计算
2. **新增 `dragOffset` 属性** — 仅用于拖拽偏移量（相对于居中位置）
3. **`dialogRect.x/y` 改为绑定表达式** — `__clampedCenterX/Y + dragOffset`
4. **新增 `__clampedCenterX` / `__clampedCenterY` 只读属性** — 带边界约束的居中坐标计算
5. **`showDialog()` 极简化** — 仅设置属性和启动动画，无位置计算
6. **`dialogRect.implicitHeight` 添加上限** — `Math.min(contentHeight + 36, parent.height * 0.9)`
7. **`contentColumn` 包裹在 Flickable 中** — 超长内容支持滚动

---

## 四、技术细节

### 尝试过的方案（按时间顺序）

| 方案 | 结果 | 原因 |
|------|------|------|
| 直接边界约束 (`Math.max/min`) | ❌ 首次仍截断 | implicitHeight 在 showDialog() 调用时尚未更新 |
| `Qt.callLater()` 延迟 | ❌ 首次仍截断 | callLater 仍在同一帧内执行，布局未完成 |
| `Timer(interval:0)` | ❌ 首次仍截断 | 单次事件循环可能不足以完成嵌套布局 |
| **响应式 Binding** | ✅ 彻底解决 | 位置由 QML 引擎在每次属性变化时自动重新计算 |

### 关键设计决策

1. **选择响应式绑定而非延迟调用**：QML 的声明式绑定系统天然解决了"何时数据就绪"的问题 — 绑定表达式会在任何依赖项变化时自动重新求值
2. **保留拖拽功能**：通过 `dragOffset` 属性实现，拖拽时修改偏移量而非绝对位置，基础居中位置始终由绑定维护
3. **Flickable 的 `interactive` 条件**：仅在 `contentHeight > height` 时启用滚动，短内容对话框不会出现不必要的滚动条

---

## 五、测试验证

| 测试场景 | 预期结果 | 实际结果 |
|----------|----------|----------|
| 首次点击"Shader - 视频"清理按钮 | 对话框完整显示在窗口内，不被截断 | ✅ 通过 |
| 点击其他清理按钮（短内容） | 对话框正常居中显示，无多余滚动 | ✅ 通过 |
| 拖拽对话框 | 拖拽过程中不超出父容器范围 | ✅ 通过 |
| 多次快速打开/关闭 | 每次打开均正确定位 | ✅ 通过 |

---

## 六、影响范围

- 影响模块：`qml/components/Dialog.qml`（通用对话框组件）
- 影响范围：所有使用 Dialog 组件的场景（设置页面、全局提示等）
- 风险评估：低风险，纯 UI 定位逻辑变更，不影响业务功能
- 兼容性：完全向后兼容，API 接口不变
