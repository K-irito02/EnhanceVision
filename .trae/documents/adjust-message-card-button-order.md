# 调整消息卡片按钮顺序计划

## 任务目标

调整 `MessageItem.qml` 中消息卡片的按钮显示顺序。

## 当前状态

**文件位置**: [MessageItem.qml](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/MessageItem.qml#L361-L404)

**当前按钮顺序**（从左到右）:
1. 暂停/继续
2. 重新处理失败文件
3. 下载（保存成功文件）
4. 删除

## 目标状态

**目标按钮顺序**（从左到右）:
1. 下载（保存成功文件）
2. 重新处理失败文件
3. 暂停/继续
4. 删除

## 实施步骤

### 步骤 1：调整按钮顺序

修改 `MessageItem.qml` 第 361-404 行的 `Row` 组件内的 `IconButton` 顺序：

将代码从：
```qml
Row {
    spacing: 2
    visible: root.totalFileCount > 0
    
    // 1. 暂停/继续按钮
    IconButton { ... }
    
    // 2. 重新处理按钮
    IconButton { ... }
    
    // 3. 下载按钮
    IconButton { ... }
    
    // 4. 删除按钮
    IconButton { ... }
}
```

调整为：
```qml
Row {
    spacing: 2
    visible: root.totalFileCount > 0
    
    // 1. 下载按钮
    IconButton { ... }
    
    // 2. 重新处理按钮
    IconButton { ... }
    
    // 3. 暂停/继续按钮
    IconButton { ... }
    
    // 4. 删除按钮
    IconButton { ... }
}
```

### 步骤 2：构建验证

使用 `qt-build-and-fix` 技能构建并运行项目，验证按钮顺序正确。

### 步骤 3：日志检查

检查运行日志，确保无警告或错误。

## 影响范围

- 仅影响 `MessageItem.qml` 文件
- 纯 UI 布局调整，不涉及业务逻辑
- 不影响按钮的显示条件或功能

## 风险评估

- **风险等级**: 低
- **影响范围**: 仅视觉布局
- **回滚方案**: 恢复原按钮顺序即可
