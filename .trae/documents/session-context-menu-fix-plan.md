# 会话区域上下文菜单优化计划

## 问题分析

### 根本原因
经过深入分析，发现以下问题：

1. **Menu vs Popup 的差异**：
   - ComboBox 使用的是 `Popup` 组件
   - SessionList 使用的是 `Menu` 组件
   - 两者的样式系统完全不同，Menu 有自己的默认样式覆盖

2. **Qt 资源系统缓存**：
   - 项目使用 `qt_add_qml_module` 编译 QML 到可执行文件
   - 修改后需要完全重新构建才能生效

3. **样式不一致**：
   - Menu 的 delegate 样式与 ComboBox 的 ItemDelegate 样式不同
   - Menu 没有阴影效果
   - Menu 的悬停效果实现方式不同

---

## 解决方案

### 方案：使用 Popup 替代 Menu

将 Menu 组件替换为自定义 Popup 组件，完全复用 ComboBox 的样式代码。

---

## 详细实现步骤

### 步骤1：创建会话上下文菜单组件

创建新文件 `qml/components/SessionContextMenu.qml`，完全复用 ComboBox 的 Popup 样式：

```qml
Popup {
    id: root
    
    property var menuItems: []
    property var sessionData: null
    
    padding: 4
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    
    background: Rectangle {
        implicitWidth: 140
        color: Theme.colors.popover
        border.width: 1
        border.color: Theme.colors.border
        radius: Theme.radius.md
        
        // 阴影效果 - 与 ComboBox 完全一致
        layer.enabled: true
        layer.effect: Item {
            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.4 : 0.1)
                radius: Theme.radius.md + 4
                z: -1
            }
        }
    }
    
    contentItem: ColumnLayout {
        spacing: 2
        
        Repeater {
            model: root.menuItems
            
            Rectangle {
                Layout.fillWidth: true
                height: 32
                radius: Theme.radius.xs
                
                // 背景色逻辑
                color: itemMouseArea.containsMouse ? Theme.colors.primary : "transparent"
                
                Behavior on color {
                    ColorAnimation { duration: Theme.animation.fast }
                }
                
                // 内容：图标 + 文字
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8
                    
                    ColoredIcon {
                        source: modelData.iconSource
                        iconSize: 14
                        color: itemMouseArea.containsMouse ? "#FFFFFF" : modelData.iconColor
                    }
                    
                    Text {
                        text: modelData.text
                        color: itemMouseArea.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    Item { Layout.fillWidth: true }
                }
                
                MouseArea {
                    id: itemMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        modelData.action()
                        root.close()
                    }
                }
            }
        }
    }
}
```

### 步骤2：修改 SessionList.qml

将 Menu 替换为自定义 Popup：

```qml
// 添加属性
property var contextMenuSession: null
property alias contextMenu: contextMenuPopup

// 替换 Menu 为 Popup
Popup {
    id: contextMenuPopup
    
    padding: 4
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    
    background: Rectangle {
        implicitWidth: 140
        color: Theme.colors.popover
        border.width: 1
        border.color: Theme.colors.border
        radius: Theme.radius.md
        
        layer.enabled: true
        layer.effect: Item {
            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.4 : 0.1)
                radius: Theme.radius.md + 4
                z: -1
            }
        }
    }
    
    contentItem: ColumnLayout {
        spacing: 2
        
        // 重命名
        Rectangle {
            Layout.fillWidth: true
            height: 32
            radius: Theme.radius.xs
            color: renameMouse.containsMouse ? Theme.colors.primary : "transparent"
            
            Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8
                
                ColoredIcon {
                    source: Theme.icon("pencil")
                    iconSize: 14
                    color: renameMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                }
                
                Text {
                    text: qsTr("重命名")
                    color: renameMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                    font.pixelSize: 13
                }
            }
            
            MouseArea {
                id: renameMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    contextMenuPopup.close()
                    delegateRoot.isEditing = true
                    nameEditField.text = model.name
                    nameEditField.forceActiveFocus()
                    nameEditField.selectAll()
                }
            }
        }
        
        // 置顶/取消置顶
        // ... 类似结构
        
        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            height: 1
            color: Theme.colors.border
        }
        
        // 清空会话
        // ... 类似结构
        
        // 删除会话
        // ... 类似结构
    }
    
    function show(sessionData, x, y) {
        contextMenuSession = sessionData
        contextMenuPopup.x = x
        contextMenuPopup.y = y
        contextMenuPopup.open()
    }
}
```

### 步骤3：修改调用方式

```qml
// 右键菜单调用
onClicked: function(mouse) {
    if (mouse.button === Qt.RightButton) {
        contextMenu.show(model, mouse.x, mouse.y)
    }
}

// 三个点按钮调用
IconButton {
    onClicked: {
        var globalPos = mapToItem(sessionListView, width, height)
        contextMenu.show(model, globalPos.x, globalPos.y)
    }
}
```

### 步骤4：修改新建会话按钮图标

在 Sidebar.qml 中，确保图标颜色为白色：

```qml
ColoredIcon {
    anchors.centerIn: parent
    source: Theme.icon("plus")
    iconSize: 16
    color: "#FFFFFF"
}
```

### 步骤5：强制重新构建

由于 Qt 资源系统缓存，需要：

1. 删除整个 build 目录
2. 重新运行 CMake 配置
3. 完整重新构建

```powershell
Remove-Item -Path "build\msvc2022" -Recurse -Force
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

---

## 文件修改清单

| 文件 | 修改内容 |
|------|----------|
| `qml/components/SessionList.qml` | 将 Menu 替换为自定义 Popup，完全复用 ComboBox 样式 |
| `qml/components/Sidebar.qml` | 确认新建会话按钮图标为白色 |

---

## 验证步骤

1. 完全删除 build 目录后重新构建
2. 启动应用程序
3. 右键点击会话标签，验证菜单样式与预设下拉框一致
4. 点击"三个点"按钮，验证菜单样式
5. 验证悬停效果：蓝色背景 + 白色图标文字
6. 验证新建会话按钮图标为白色
7. 切换深色/浅色主题，验证样式一致性

---

## 风险评估

- **中等风险**：需要完全重新构建，构建时间较长
- **兼容性**：Popup 和 Menu 的 API 不同，需要调整调用方式
- **回滚**：如有问题可恢复原始 Menu 实现
