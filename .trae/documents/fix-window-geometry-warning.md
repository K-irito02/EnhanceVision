# 解决 Window setGeometry 警告

## 问题分析

### 警告原因
```
QWindowsWindow::setGeometry: Unable to set geometry 400x272+1019+438
Resulting geometry: 400x300+1019+438
```

Windows 窗口管理器对窗口最小高度有限制（约 300px），当 QML 窗口尝试设置高度为 272px 时被强制调整为 300px，触发警告。

### 触发场景
"保存风格"对话框 (`savePresetDialog`) 中：
- 窗口高度通过 `height: savePresetContent.implicitHeight + 40 + 32` 动态计算
- `categorySection` 高度根据下拉菜单状态变化：
  - 收起时：40px
  - 展开时：40 + 80 + 4 = 124px
- 当下拉菜单收起时，窗口高度可能小于 Windows 最小高度限制

### 受影响的对话框
根据代码分析，以下对话框都使用类似的动态高度计算：
1. `savePresetDialog` - 保存风格（有下拉菜单，主要问题源）
2. `deleteCategoryDialog` - 删除类别
3. `addCategoryDialog` - 新建类别
4. `renameCategoryDialog` - 重命名类别
5. `renamePresetDialog` - 重命名风格

## 解决方案

为所有对话框添加 `minimumHeight` 属性，确保窗口高度不小于 Windows 系统限制。

### 实施步骤

1. **计算合理的最小高度**
   - 标题栏高度：约 40px
   - 内容区域最小高度：约 200px（输入框 + 标签 + 按钮）
   - 底部间距：32px
   - 建议最小高度：280px（略低于 300px 限制，留出安全边距）

2. **修改 `savePresetDialog`**
   - 添加 `minimumHeight: 300` 属性
   - 确保下拉菜单展开/收起时窗口高度变化平滑

3. **修改其他对话框**
   - 为 `deleteCategoryDialog`、`addCategoryDialog`、`renameCategoryDialog`、`renamePresetDialog` 添加 `minimumHeight: 300`

## 预期结果

- 警告消失
- 下拉菜单展开时窗口底部和按钮正常向下移动
- 下拉菜单收起时窗口高度不会小于系统限制
