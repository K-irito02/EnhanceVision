# SVG 图标显示修复

## 概述

修复了应用程序中所有 SVG 图标无法正确显示的问题，图标现在可以根据主题自动显示正确的颜色。

**创建日期**: 2026-03-20
**作者**: AI Assistant

---

## 一、问题描述

### 现象

1. 应用程序中所有 SVG 图标都不显示
2. 暗色主题下图标显示为黑色，在深色背景下不清晰
3. 运行时日志显示 `Cannot open: qrc:/icons-dark/x.svg` 错误

### 原因分析

1. **qrc 资源路径配置错误**：
   - 原配置：`prefix="/icons"` + `file="icons/home.svg"` → 实际路径 `qrc:/icons/icons/home.svg`
   - 期望路径：`qrc:/icons/home.svg`

2. **SVG 颜色问题**：
   - `icons/` 文件夹使用 `stroke="currentColor"`，Qt SVG 对此支持有限
   - `icons-dark/` 文件夹使用硬编码 `stroke="#7AA2F7"`

3. **MultiEffect colorization 机制**：
   - `MultiEffect` 的 colorization 使用颜色乘法算法
   - 黑色 (`#000000`) 乘以任何颜色都是黑色，无法被着色
   - 白色 (`#FFFFFF`) 可以被正确着色为任何颜色

---

## 二、解决方案

### 1. 修复 qrc 资源路径

**文件**: `resources/qml.qrc`

```xml
<!-- 修改前 -->
<qresource prefix="/icons">
    <file>icons/home.svg</file>
</qresource>

<!-- 修改后 -->
<qresource prefix="/">
    <file>icons/home.svg</file>
</qresource>
```

### 2. 修改 SVG 文件颜色

将所有 SVG 文件的 `stroke` 颜色改为白色 `#FFFFFF`：

```powershell
# icons-dark 文件夹
Get-ChildItem -Path "resources\icons-dark" -Filter "*.svg" | ForEach-Object {
    (Get-Content $_.FullName -Raw) -replace 'stroke="currentColor"', 'stroke="#FFFFFF"' |
    Set-Content $_.FullName -NoNewline
}

# icons 文件夹
Get-ChildItem -Path "resources\icons" -Filter "*.svg" | ForEach-Object {
    (Get-Content $_.FullName -Raw) -replace 'stroke="currentColor"', 'stroke="#FFFFFF"' |
    Set-Content $_.FullName -NoNewline
}
```

### 3. 修复 SettingsPage.qml

将直接使用 `Image` 的地方改为 `ColoredIcon`：

```qml
// 修改前
Image { width: 18; height: 18; source: Theme.icon("monitor"); sourceSize: Qt.size(18, 18); smooth: true }

// 修改后
ColoredIcon { iconSize: 18; source: Theme.icon("monitor"); color: Theme.colors.icon }
```

---

## 三、技术实现细节

### ColoredIcon 组件

**文件**: `qml/controls/ColoredIcon.qml`

```qml
Item {
    id: root
    
    property string source: ""
    property int iconSize: 18
    property color color: Theme.colors.icon
    
    Image {
        id: iconImage
        source: root.source
        sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
        layer.enabled: true
        layer.effect: MultiEffect {
            colorizationColor: root.color
            colorization: 1.0
        }
    }
}
```

### 颜色叠加原理

```
SVG 白色 (#FFFFFF) × Theme.colors.icon (#7AA2F7) = 淡蓝色图标
SVG 白色 (#FFFFFF) × Theme.colors.icon (#5A6A85) = 蓝灰色图标
```

### 主题颜色配置

**文件**: `qml/styles/Colors.qml`

| 主题 | 图标颜色 | 说明 |
|------|---------|------|
| 暗色 | `#7AA2F7` | 淡蓝色 |
| 亮色 | `#5A6A85` | 蓝灰色 |

---

## 四、修改的文件

| 文件 | 修改内容 |
|------|---------|
| `resources/qml.qrc` | 修复图标资源路径 prefix |
| `resources/icons/*.svg` | stroke 颜色改为 #FFFFFF |
| `resources/icons-dark/*.svg` | stroke 颜色改为 #FFFFFF |
| `qml/pages/SettingsPage.qml` | Image 改为 ColoredIcon |

---

## 五、验证结果

- ✅ 所有图标正确显示
- ✅ 暗色主题下图标显示淡蓝色
- ✅ 亮色主题下图标显示蓝灰色
- ✅ 主题切换时图标颜色自动更新
- ✅ 无运行时错误
