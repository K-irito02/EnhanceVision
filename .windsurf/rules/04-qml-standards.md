---
alwaysApply: false
globs: ['**/*.qml', '**/*.js']
description: 'QML代码规范 - 组件化、绑定、信号、资源引用'
trigger: glob
---
# QML 代码规范

## 命名规范

| 类型 | 风格 | 示例 |
|------|------|------|
| QML 文件 | PascalCase | `FileList.qml` |
| id | camelCase | `fileList` |
| 属性 | camelCase | `currentIndex` |
| 私有状态 | `_`前缀 | `_internalState` |

## 组件结构顺序

```
1. id
2. 公共属性
3. 私有属性
4. 别名
5. 信号
6. 方法
7. Connections
8. 子组件
```

## 绑定规范

1. **优先声明式绑定**：避免命令式赋值
2. **复杂表达式提取函数**：或下沉到 C++
3. **绑定中添加空值检查**：`xxx !== undefined && xxx !== null`

## 信号与交互

1. **信号语义明确**：参数最小化
2. **Connections 仅处理本组件事件**
3. **交互逻辑避免分散**：集中在一个处理器

## JavaScript 边界

1. **JS 仅处理轻量 UI 逻辑**
2. **业务逻辑、IO、重计算放 C++**
3. **延迟更新使用 `Qt.callLater()`**
4. **禁止在 JS 中创建复杂对象**

## 资源与文本

1. **资源统一使用 QRC / Provider**
2. **用户可见文本必须 `qsTr()`**
3. **图标使用 `Theme.icon()` + `ColoredIcon`**

## 性能规范

1. **图像设置 `sourceSize`**：禁止无控制加载原图
2. **列表使用 `ListView`**：避免大规模 `Repeater`
3. **重组件使用 `Loader`**：延迟加载

## 布局与状态管理

1. **布局变化监听**：使用属性变化监听器而非直接事件处理
2. **状态锁定机制**：复杂状态切换时使用锁定标志避免冲突
3. **延迟布局更新**：使用 `Timer` 确保布局计算完成后再操作
4. **滚动控制**：`ListView` 滚动使用专用方法，避免直接操作 `contentY`
5. **条件渲染**：根据业务状态动态控制组件可见性
