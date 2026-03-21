# AI 推理控制面板 UI 优化

## 概述

修复 AI 推理控制面板的多个 UI 问题，包括国际化支持、滚动区域冲突、文本溢出、开关样式和滑动条样式优化。

**创建日期**: 2026-03-21
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `src/core/ModelRegistry.cpp` | 添加 `name_en` 字段加载，支持类别国际化 |
| `qml/components/ControlPanel.qml` | 移除 AI 模式外层 ScrollView，修复滚动冲突 |
| `qml/components/AIParamsPanel.qml` | 优化模型信息卡片布局、GPU 状态标签、使用自定义 Switch 和 Slider |
| `qml/components/AIModelPanel.qml` | 删除调试日志代码 |
| `qml/components/MessageItem.qml` | 修复状态标签和模式标签文本溢出问题 |
| `qml/components/SessionBatchBar.qml` | 删除调试日志代码 |
| `qml/controls/Switch.qml` | 创建自定义开关组件，优化尺寸和颜色 |
| `qml/controls/Slider.qml` | 优化滑动条样式和滑动按钮尺寸 |
| `qml/styles/Colors.qml` | 添加 `switchOn` 和 `switchOff` 颜色属性 |
| `resources/i18n/app_zh_CN.ts` | 添加"选中模型"翻译 |
| `resources/i18n/app_en_US.ts` | 添加"Selected Model"翻译 |
| `CMakeLists.txt` | 注册 Switch.qml 到 QML_FILES |

### 2. 实现的功能特性

- ✅ 类别和模型列表支持中英语言切换
- ✅ 修复滚动区域冲突，滚动只影响模型列表区域
- ✅ 模型设置区域自适应布局，消除底部空白
- ✅ "选中模型"文本实时语言切换
- ✅ 标注角布局优化，确保不溢出边框
- ✅ GPU 状态标签文本自适应宽度
- ✅ 消息状态标签文本自适应宽度
- ✅ 自定义 Switch 组件，优化尺寸和颜色
- ✅ 自定义 Slider 组件，优化样式和交互

---

## 二、遇到的问题及解决方案

### 问题 1：类别国际化不生效

**现象**：切换语言时，类别名称没有更新为对应语言

**原因**：`ModelRegistry::loadModelsJson()` 加载类别元数据时缺少 `name_en` 字段

**解决方案**：在加载类别元数据时添加 `name_en` 字段保存

### 问题 2：滚动区域冲突

**现象**：鼠标滚轮和拖拽滚动条影响整个面板，而不是只影响模型列表区域

**原因**：`ControlPanel.qml` 的 AI 模式使用 `ScrollView` 包裹 `AIModelPanel`，而 `AIModelPanel` 内部又有自己的 `ListView`，导致嵌套滚动冲突

**解决方案**：移除外层 `ScrollView`，让 `AIModelPanel` 自己管理滚动

### 问题 3：文本溢出边框

**现象**：英文环境下，"Unavailable" 和 "Completed" 文本超出背景边界

**原因**：标签使用固定 `width` 而不是自适应宽度

**解决方案**：使用 `Layout.minimumWidth` 和 `Layout.preferredWidth` 替代固定 `width`，并增加内边距

### 问题 4：自定义组件未生效

**现象**：自定义 Switch 和 Slider 样式没有应用

**原因**：AIParamsPanel.qml 中使用的是 Qt 默认的 `Switch` 和 `Slider`，而不是自定义的 `Controls.Switch` 和 `Controls.Slider`

**解决方案**：将 `Switch` 改为 `Controls.Switch`，将 `Slider` 改为 `Controls.Slider`

---

## 三、技术实现细节

### 自定义 Switch 组件

```qml
// qml/controls/Switch.qml
Item {
    id: control
    implicitWidth: 40
    implicitHeight: 22

    property bool checked: false
    signal toggled()

    Rectangle {
        anchors.fill: parent
        radius: 11
        color: control.checked ? Theme.colors.switchOn : Theme.colors.switchOff

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: parent.height / 2 - height / 2
            width: 16
            height: 16
            radius: 8
            color: "#FFFFFF"
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            control.checked = !control.checked
            control.toggled()
        }
    }
}
```

### 自定义 Slider 组件

```qml
// qml/controls/Slider.qml
Slider {
    id: root

    background: Rectangle {
        height: 4
        radius: 2
        // 轨道样式...
    }

    handle: Rectangle {
        width: 14
        height: 14
        radius: 7
        color: "#FFFFFF"
        border.width: 2
        border.color: Theme.colors.primary
        // 滑块样式...
    }
}
```

### 标签自适应宽度

```qml
Rectangle {
    height: 18
    radius: Theme.radius.sm
    Layout.minimumWidth: labelText.implicitWidth + 16
    Layout.preferredWidth: labelText.implicitWidth + 16

    Text {
        id: labelText
        anchors.centerIn: parent
        text: qsTr("文本内容")
    }
}
```

---

## 四、颜色定义

### 开关颜色

| 状态 | 暗色主题 | 亮色主题 |
|------|---------|---------|
| 开启 | `#3B82F6` | `#5B8DEF` |
| 关闭 | `#4A5568` | `#A8B8D0` |

---

## 五、删除的调试代码

### AIModelPanel.qml

移除了以下 `console.log` 调用：
- `[AIModelPanel] Language changed, refreshing data...`
- `[AIModelPanel] Refreshing categories...`
- `[AIModelPanel] Refreshing models for category:`
- `[AIModelPanel] Component completed, registry:`
- `[AIModelPanel] Categories count:`
- `[AIModelPanel] Selected category:`
- `[AIModelPanel] Models count:`

### SessionBatchBar.qml

移除了以下 `console.log` 调用：
- `[SessionBatchBar] Batch buttons row created, batchMode:`
- `[SessionBatchBar] trash-2 button created, enabled:`
- `[SessionBatchBar] x-circle button created, enabled:`
