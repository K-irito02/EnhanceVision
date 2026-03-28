# 消息卡片右键菜单重构笔记

## 概述

重构消息卡片界面中多媒体文件的右键菜单，使其与会话标签的右键菜单样式一致，并删除冗余的调试信息。

**创建日期**: 2026-03-29
**作者**: AI Assistant

---

## 一、变更概述

### 1. 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/MediaThumbnailStrip.qml` | 将 Menu 替换为 Popup，重构菜单样式 |
| `src/core/AIEngine.cpp` | 删除冗余调试信息，修复编译错误 |
| `src/core/video/AIVideoProcessor.cpp` | 删除冗余调试信息 |
| `src/core/video/VideoResourceGuard.cpp` | 删除冗余调试信息 |
| `src/core/AIEnginePool.cpp` | 删除冗余调试信息 |

---

## 二、实现的功能特性

- ✅ 右键菜单样式与会话标签一致（阴影、圆角、动画效果）
- ✅ 菜单高度与内容贴合，无多余空白
- ✅ 三种状态（正在处理、等待处理、处理失败）的菜单显示正确
- ✅ 删除冗余调试信息，减少日志输出

---

## 三、技术实现细节

### 菜单重构

将 `Menu` 组件替换为 `Popup` 组件，实现与会话标签一致的样式：

1. **背景样式**：
   - 使用 `Theme.radius.lg` 圆角
   - 添加阴影效果（`Qt.rgba(0, 0, 0, Theme.isDark ? 0.4 : 0.1)`）
   - 边框颜色使用 `Theme.colors.border`

2. **菜单项样式**：
   - 高度统一为 34px
   - 使用 `RowLayout` 布局
   - 图标大小 16px，间距 10px
   - 鼠标悬停时背景色变化（`Theme.colors.primary` 或 `Theme.colors.destructive`）

3. **动画效果**：
   - 使用 `Behavior on color { ColorAnimation { duration: Theme.animation.fast } }` 实现平滑过渡

4. **分隔线**：
   - 根据菜单项可见性动态显示
   - 高度 1px，左右边距 8px

### showAt 函数

实现类似会话标签的 `showAt` 函数，支持全局坐标定位：

```qml
function showAt(globalX, globalY) {
    var menuWidth = 160
    var menuHeight = calculateMenuHeight()
    var margin = 8
    var offsetX = 4
    var offsetY = 4
    
    var overlayPos = parent.mapFromGlobal(globalX, globalY)
    
    // 边界检测和位置调整
    // ...
    
    contextMenu.x = finalX
    contextMenu.y = finalY
    contextMenu.open()
}
```

### 调试信息清理

删除以下类型的冗余调试信息：
- 循环中频繁输出的日志
- 重复的状态变更日志
- 不必要的性能日志

保留的日志：
- 关键生命周期事件（启动、关闭）
- 错误和警告信息
- 性能阈值超限日志

---

## 四、遇到的问题及解决方案

### 问题 1：编译错误 - loadedLayerCount 未声明

**现象**：`AIEngine.cpp:213: error C2065: "loadedLayerCount": 未声明的标识符`

**原因**：代码中使用了未定义的变量 `loadedLayerCount`

**解决方案**：使用 NCNN Net 类的 `layers()` 方法获取层数：
```cpp
m_currentModel.layerCount = static_cast<int>(m_net.layers().size());
```

### 问题 2：菜单高度不正确

**现象**：当只有"删除"菜单项时，菜单高度有多余空白

**原因**：`MenuSeparator` 的 `visible` 属性硬编码为 `true`

**解决方案**：重构为 Popup 后，使用 `ColumnLayout` 自动处理可见性

---

## 五、测试验证

### 测试场景

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 正在处理状态右键 | 只显示"删除" | ✅ 通过 |
| 等待处理状态右键 | 只显示"删除" | ✅ 通过 |
| 处理成功状态右键 | 显示"放大查看"、"保存"、"删除" | ✅ 通过 |
| 处理失败状态右键 | 显示"重新处理"、"删除" | ✅ 通过 |
| 菜单样式一致性 | 与会话标签一致 | ✅ 通过 |

---

## 六、性能影响

| 指标 | 变更前 | 变更后 | 影响 |
|------|--------|--------|------|
| 日志输出量 | 较多 | 减少 | 减少 I/O |
| 菜单渲染 | Menu 组件 | Popup 组件 | 无明显差异 |

---

## 七、后续工作

- [ ] 无
