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
3. **Theme SVG 图标必须统一走 `Theme.icon()` + `ColoredIcon`**：禁止在业务页面里直接拼接图标路径或重复注册同一套 SVG 资源
4. **位图/品牌 Logo 仍然使用 `Image`**：应用 Logo、固定颜色图片、非主题着色资源不要混入 `ColoredIcon`

## 性能规范

1. **图像设置 `sourceSize`**：禁止无控制加载原图
2. **列表使用 `ListView`**：避免大规模 `Repeater`
3. **重组件使用 `Loader`**：延迟加载

## 运行时告警规避

1. **Singleton 必须限定名访问**：本地目录导入单例时使用 `import "../utils" as Utils` 并通过 `Utils.ResponsiveUtils` 访问，避免 `ReferenceError`。
2. **信号处理器禁止隐式注入参数**：使用 `onSignal: function(arg1, arg2) { ... }`，避免 Qt 6 的参数注入弃用告警。
3. **按媒体类型绑定 `Image.source`**：`visible: false` 不能阻止资源加载，视频场景必须将 `source` 绑定为空字符串，避免 `QQuickImage` 误加载 mp4 告警。
4. **Menu 动态项 API 区分**：`MenuItem` 使用 `insertItem/removeItem`，`Menu` 使用 `insertMenu/removeMenu`，避免 JS 到 C++ 参数类型错误。
