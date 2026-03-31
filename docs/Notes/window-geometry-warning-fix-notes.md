# Window setGeometry 警告修复笔记

## 概述

修复 Windows 平台上 QML 对话框触发 `QWindowsWindow::setGeometry` 警告的问题。

**创建日期**: 2026-04-01
**作者**: AI Assistant

---

## 一、问题描述

### 问题现象

程序运行时日志中出现以下警告：

```
[WARN] QWindowsWindow::setGeometry: Unable to set geometry 400x272+1019+438 (frame: 400x272+1019+438) on QQuickWindowQmlImpl_QML_111/"" on "24G4". Resulting geometry: 400x300+1019+438 (frame: 400x300+1019+438) margins: 0, 0, 0, 0)
```

### 触发场景

在"保存风格"对话框中，当下拉菜单收起时，窗口高度计算值小于 Windows 系统最小高度限制（约 300px），导致窗口管理器强制调整高度并触发警告。

---

## 二、根因分析

### 问题原因

1. **动态高度计算**：对话框高度通过 `height: content.implicitHeight + 40 + 32` 动态计算
2. **下拉菜单影响**：下拉菜单展开/收起会改变 `implicitHeight`
3. **系统限制**：Windows 窗口管理器对窗口最小高度有限制（约 300px）
4. **高度不足**：下拉菜单收起时，计算高度为 272px，小于系统限制

### 受影响的对话框

| 对话框 | 文件位置 |
|--------|----------|
| `savePresetDialog` | 保存风格 |
| `deleteCategoryDialog` | 删除类别 |
| `addCategoryDialog` | 新建类别 |
| `renameCategoryDialog` | 重命名类别 |
| `renamePresetDialog` | 重命名风格 |

---

## 三、解决方案

### 修复方法

使用 `Math.max()` 确保窗口高度不小于系统最小限制：

```qml
// 修复前
height: savePresetContent.implicitHeight + 40 + 32
minimumHeight: 300

// 修复后
height: Math.max(savePresetContent.implicitHeight + 40 + 32, 300)
```

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/ShaderParamsPanel.qml` | 5 个对话框的高度计算逻辑 |

---

## 四、技术细节

### 为什么 minimumHeight 不够？

`minimumHeight` 只是设置约束属性，但 `height` 绑定仍会尝试设置为计算值。当计算值小于 `minimumHeight` 时，Qt 会尝试设置 `height` 为计算值，然后被系统限制，触发警告。

### 正确做法

在绑定表达式中使用 `Math.max()`，从源头确保高度值不小于最小限制，避免触发系统调整。

---

## 五、测试验证

| 测试场景 | 预期结果 | 实际结果 |
|----------|----------|----------|
| 打开"保存风格"对话框 | 无警告 | ✅ 通过 |
| 展开/收起下拉菜单 | 窗口高度动态变化，无警告 | ✅ 通过 |
| 其他对话框操作 | 无警告 | ✅ 通过 |

---

## 六、影响范围

- 影响模块：QML 对话框组件
- 影响功能：对话框高度计算
- 风险评估：低（仅修改高度计算逻辑，不影响功能）
