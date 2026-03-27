---
alwaysApply: false
globs: ['**/*.qml', '**/*.js']
description: 'QML代码规范 - 组件化、绑定、信号、资源引用与JS边界'
trigger: glob
---
# QML 代码规范

## 命名与文件

| 类型 | 风格 | 示例 |
|------|------|------|
| QML 文件 | PascalCase | `FileList.qml` |
| id | camelCase | `fileList` |
| 属性 | camelCase | `currentIndex` |
| 私有状态 | `_`前缀 | `_internalState` |

## 组件结构

推荐顺序：`属性 → 信号 → 连接 → 函数 → 子组件`

```qml
Item {
    id: root
    
    // 1. 公共属性
    property string title: ""
    
    // 2. 私有属性
    property var _internalData: null
    
    // 3. 别名
    property alias buttonText: button.text
    
    // 4. 信号
    signal clicked()
    
    // 5. 方法
    function doSomething() { }
    
    // 6. 子组件
    Button { id: button }
}
```

## 绑定与状态

- ✅ 优先声明式绑定
- ✅ 复杂表达式提取函数或下沉 C++
- ❌ 避免在绑定中进行重计算

## 信号与交互

- 信号语义明确、参数最小
- `Connections` 仅处理本组件相关事件
- 交互逻辑避免分散到多个匿名处理器

## JavaScript 边界

- JS 仅处理轻量 UI 逻辑
- 业务逻辑、IO、重计算放 C++

## 资源与文本

- 资源统一使用 QRC / Provider
- 用户可见文本必须 `qsTr()`

## 本文件边界

- 仅定义 QML/JS 编码规范
- 视觉规范见 `08-ui-design.md`
