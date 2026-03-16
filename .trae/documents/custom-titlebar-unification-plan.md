# 自定义标题栏统一实施方案

## 一、需求概述

将项目中所有使用原生 Windows 标题栏的窗口改为自定义标题栏，与主程序界面标题栏样式一致。

### 需要修改的窗口

| 窗口 | 文件 | 当前状态 | 用途 |
|------|------|---------|------|
| 媒体查看器窗口 | MediaViewerWindow.qml | 原生标题栏 | 查看/播放媒体文件 |
| 删除类别对话框 | ShaderParamsPanel.qml | 原生标题栏 | 删除类别确认 |
| 新建类别对话框 | ShaderParamsPanel.qml | 原生标题栏 | 创建新类别 |
| 重命名类别对话框 | ShaderParamsPanel.qml | 原生标题栏 | 重命名类别 |
| 重命名风格对话框 | ShaderParamsPanel.qml | 原生标题栏 | 重命名风格预设 |
| 保存风格对话框 | ShaderParamsPanel.qml | 原生标题栏 | 保存风格预设 |

## 二、技术分析

### 当前架构

1. **主窗口**：使用 `QQuickWidget` 嵌入 QML，通过 `WindowHelper` 单例处理 Windows 原生事件
2. **子窗口**：使用 QML `Window` 组件，使用原生标题栏
3. **WindowHelper**：单例模式，只能绑定一个主窗口

### 核心挑战

1. **WindowHelper 单例限制**：当前设计只支持一个主窗口
2. **子窗口独立生命周期**：子窗口动态创建和销毁
3. **样式一致性**：所有窗口需要使用相同的主题样式

## 三、解决方案

### 方案设计原则

1. **组件复用**：创建可复用的标题栏组件
2. **最小改动**：尽量复用现有 WindowHelper 代码
3. **易维护**：清晰的代码结构和命名规范
4. **向后兼容**：不影响主窗口现有功能

### 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                      窗口层级架构                             │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────┐     ┌─────────────────────────────┐   │
│  │   MainWindow    │     │      SubWindow (QML)        │   │
│  │  (QQuickWidget) │     │  ┌───────────────────────┐  │   │
│  │                 │     │  │  DialogTitleBar       │  │   │
│  │  ┌───────────┐  │     │  │  - 标题文本            │  │   │
│  │  │ TitleBar  │  │     │  │  - 关闭按钮            │  │   │
│  │  └───────────┘  │     │  │  - 拖拽支持            │  │   │
│  │                 │     │  └───────────────────────┘  │   │
│  │  WindowHelper   │     │  SubWindowHelper (C++)     │   │
│  │  (单例，主窗口)  │     │  (每个子窗口一个实例)       │   │
│  └─────────────────┘     └─────────────────────────────┘   │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 组件设计

#### 1. DialogTitleBar.qml（新建）

可复用的对话框标题栏组件：

```qml
// qml/components/DialogTitleBar.qml
Rectangle {
    id: root
    
    // 公开属性
    property string titleText: ""
    property bool showMinimize: false
    property bool showMaximize: false
    property bool isMaximized: false
    property bool showDivider: true
    
    // 信号
    signal closeClicked()
    signal minimizeClicked()
    signal maximizeClicked()
    
    // 高度 40px，适合对话框
    height: 40
    color: Theme.colors.titleBar
    
    // 布局：标题 + 窗口控制按钮
    RowLayout {
        anchors.fill: parent
        
        // 标题文本
        Text {
            text: root.titleText
            color: Theme.colors.foreground
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
        
        // 拖拽区域（填充剩余空间）
        Item { Layout.fillWidth: true }
        
        // 窗口控制按钮
        Row {
            // 最小化、最大化、关闭按钮
        }
    }
}
```

#### 2. SubWindowHelper（新建 C++ 类）

子窗口辅助类，每个子窗口创建一个实例：

```cpp
// include/EnhanceVision/utils/SubWindowHelper.h
class SubWindowHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizedChanged)
    
public:
    explicit SubWindowHelper(QObject* parent = nullptr);
    ~SubWindowHelper();
    
    Q_INVOKABLE void setWindow(QQuickWindow* window);
    Q_INVOKABLE void minimize();
    Q_INVOKABLE void maximize();
    Q_INVOKABLE void restore();
    Q_INVOKABLE void toggleMaximize();
    Q_INVOKABLE void close();
    Q_INVOKABLE bool isMaximized() const;
    Q_INVOKABLE void startSystemMove();
    
signals:
    void maximizedChanged();
    
private:
    QQuickWindow* m_window = nullptr;
    bool m_isMaximized = false;
};
```

#### 3. 修改现有窗口

将所有使用原生标题栏的窗口改为自定义标题栏：

```qml
// 修改前
Window {
    title: qsTr("删除类别")
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    
    // 内容...
}

// 修改后
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"  // 允许圆角
    
    // 自定义标题栏
    DialogTitleBar {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        titleText: qsTr("删除类别")
        onCloseClicked: root.close()
    }
    
    // 内容区域
    Rectangle {
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        
        // 原有内容...
    }
}
```

## 四、实施步骤

### 第一阶段：创建基础设施

| 步骤 | 任务 | 文件 |
|------|------|------|
| 1.1 | 创建 DialogTitleBar.qml 组件 | qml/components/DialogTitleBar.qml |
| 1.2 | 创建 SubWindowHelper C++ 类 | include/EnhanceVision/utils/SubWindowHelper.h |
| 1.3 | 实现 SubWindowHelper | src/utils/SubWindowHelper.cpp |
| 1.4 | 注册 SubWindowHelper 到 QML | CMakeLists.txt, main.cpp |

### 第二阶段：修改 MediaViewerWindow

| 步骤 | 任务 | 说明 |
|------|------|------|
| 2.1 | 移除原生标题栏 | 修改 flags 为 FramelessWindowHint |
| 2.2 | 添加自定义标题栏 | 集成 DialogTitleBar |
| 2.3 | 实现窗口拖拽 | 使用 startSystemMove() |
| 2.4 | 实现窗口控制 | 最小化、最大化、关闭 |
| 2.5 | 实现边缘调整大小 | 可选，使用 SubWindowHelper |

### 第三阶段：修改 ShaderParamsPanel 对话框

| 步骤 | 任务 | 说明 |
|------|------|------|
| 3.1 | 修改 deleteCategoryDialog | 添加自定义标题栏 |
| 3.2 | 修改 addCategoryDialog | 添加自定义标题栏 |
| 3.3 | 修改 renameCategoryDialog | 添加自定义标题栏 |
| 3.4 | 修改 renamePresetDialog | 添加自定义标题栏 |
| 3.5 | 修改 savePresetDialog | 添加自定义标题栏 |

### 第四阶段：测试和优化

| 步骤 | 任务 | 说明 |
|------|------|------|
| 4.1 | 编译测试 | 确保无编译错误 |
| 4.2 | 功能测试 | 测试所有窗口功能 |
| 4.3 | 样式测试 | 确保主题切换正常 |
| 4.4 | 性能测试 | 确保无内存泄漏 |

## 五、文件清单

### 新建文件

| 文件 | 说明 |
|------|------|
| qml/components/DialogTitleBar.qml | 对话框标题栏组件 |
| include/EnhanceVision/utils/SubWindowHelper.h | 子窗口辅助类头文件 |
| src/utils/SubWindowHelper.cpp | 子窗口辅助类实现 |

### 修改文件

| 文件 | 修改内容 |
|------|---------|
| CMakeLists.txt | 添加新源文件 |
| src/main.cpp | 注册 SubWindowHelper 类型 |
| qml/components/MediaViewerWindow.qml | 使用自定义标题栏 |
| qml/components/ShaderParamsPanel.qml | 5 个对话框使用自定义标题栏 |

## 六、DialogTitleBar 详细设计

### 属性定义

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| titleText | string | "" | 标题文本 |
| showMinimize | bool | false | 是否显示最小化按钮 |
| showMaximize | bool | false | 是否显示最大化按钮 |
| isMaximized | bool | false | 是否最大化状态 |
| showDivider | bool | true | 是否显示底部分隔线 |
| height | int | 40 | 标题栏高度 |

### 信号定义

| 信号 | 说明 |
|------|------|
| closeClicked() | 关闭按钮点击 |
| minimizeClicked() | 最小化按钮点击 |
| maximizeClicked() | 最大化按钮点击 |

### 视觉规范

```
┌────────────────────────────────────────────────────────────┐
│ ┌──────┐                                           ┌────┐ │
│ │ 图标 │  标题文本                    [─][□][×]   │关闭│ │
│ └──────┘                                           └────┘ │
├────────────────────────────────────────────────────────────┤
│                                                            │
│                      窗口内容区域                           │
│                                                            │
└────────────────────────────────────────────────────────────┘

高度：40px
背景色：Theme.colors.titleBar
分隔线：Theme.colors.titleBarBorder，高度 1px
按钮大小：32x32
按钮间距：2px
```

## 七、SubWindowHelper 详细设计

### 接口设计

```cpp
class SubWindowHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizedChanged)
    
public:
    // 构造/析构
    explicit SubWindowHelper(QObject* parent = nullptr);
    ~SubWindowHelper() override;
    
    // 窗口绑定
    Q_INVOKABLE void setWindow(QQuickWindow* window);
    
    // 窗口控制
    Q_INVOKABLE void minimize();
    Q_INVOKABLE void maximize();
    Q_INVOKABLE void restore();
    Q_INVOKABLE void toggleMaximize();
    Q_INVOKABLE void close();
    
    // 状态查询
    Q_INVOKABLE bool isMaximized() const;
    
    // 拖拽支持
    Q_INVOKABLE void startSystemMove();
    
signals:
    void maximizedChanged();
    
private:
    QQuickWindow* m_window = nullptr;
    bool m_isMaximized = false;
    
#ifdef Q_OS_WIN
    // Windows 特定实现
    void setupWindowFrame();
#endif
};
```

### Windows 平台实现要点

1. **圆角窗口**：使用 DwmSetWindowAttribute 设置 DWMWCP_ROUND
2. **无边框**：设置 WS_THICKFRAME 样式
3. **拖拽支持**：使用 startSystemMove() 或 Windows 消息
4. **最小尺寸**：处理 WM_GETMINMAXINFO 消息

## 八、注意事项

1. **主题一致性**：所有窗口使用 Theme 单例，确保主题切换时样式同步
2. **国际化**：标题文本使用 qsTr() 支持多语言
3. **内存管理**：SubWindowHelper 使用 QObject 父子关系管理生命周期
4. **性能优化**：避免频繁创建销毁 SubWindowHelper 实例
5. **向后兼容**：主窗口继续使用 WindowHelper 单例

## 九、预期效果

- ✅ 所有窗口使用统一的自定义标题栏
- ✅ 标题栏样式与主题一致
- ✅ 支持拖拽移动窗口
- ✅ 支持最小化、最大化、关闭
- ✅ 窗口有圆角效果
- ✅ 代码结构清晰，易于维护

---

**创建日期**: 2026-03-16  
**作者**: AI Assistant
