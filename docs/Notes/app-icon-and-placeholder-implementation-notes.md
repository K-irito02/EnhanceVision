# 应用程序图标与缩略图占位图自定义实现笔记

## 概述

实现应用程序 Logo 图标设置和缩略图占位图自定义功能，使用户可以使用自己的图片资源。

**创建日期**: 2026-04-02
**作者**: AI Assistant

---

## 一、变更概述

### 1. 新增文件

| 文件路径 | 功能描述 |
|----------|----------|
| `resources/icons/app_icon.png` | 应用程序 Logo（圆角，256x256） |
| `resources/icons/app_icon.ico` | Windows 应用程序图标（多尺寸） |
| `resources/icons/placeholder.png` | 自定义缩略图占位图 |
| `resources/app.rc` | Windows 资源文件，定义应用程序图标 |

### 2. 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `resources/qml.qrc` | 添加新图标资源到资源文件 |
| `CMakeLists.txt` | 添加 Windows 资源文件引用 |
| `src/main.cpp` | 设置应用程序窗口图标 |
| `include/EnhanceVision/providers/ThumbnailProvider.h` | 添加 `loadPlaceholderFromResource()` 方法声明 |
| `src/providers/ThumbnailProvider.cpp` | 实现自定义占位图加载逻辑 |
| `qml/components/TitleBar.qml` | 使用自定义 Logo 替换渐变矩形 |
| `qml/pages/MainPage.qml` | 使用自定义 Logo 替换欢迎界面图标 |

---

## 二、实现的功能特性

- ✅ **应用程序图标**：Windows 任务栏、窗口标题栏显示自定义 Logo
- ✅ **圆角 Logo**：Logo 图片自动裁剪为圆角矩形
- ✅ **标题栏 Logo**：标题栏左侧显示自定义 Logo
- ✅ **欢迎界面 Logo**：首次打开应用时中间区域显示自定义 Logo
- ✅ **自定义缩略图占位图**：缩略图加载时显示用户自定义的占位图

---

## 三、技术实现细节

### 3.1 Logo 图片格式转换

使用 Python + Pillow 库将 JPG 格式转换为 PNG 和 ICO 格式：

```python
from PIL import Image, ImageDraw

# 圆角处理
def add_rounded_corners(image, radius_ratio=0.15):
    width, height = image.size
    radius = int(min(width, height) * radius_ratio)
    
    mask = Image.new('L', (width, height), 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle([(0, 0), (width, height)], radius=radius, fill=255)
    
    result = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    result.paste(image, (0, 0))
    result.putalpha(mask)
    
    return result

# 生成多尺寸 ICO
sizes = [(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
```

### 3.2 Windows 应用程序图标设置

创建 `resources/app.rc` 资源文件：

```rc
IDI_ICON1 ICON "icons/app_icon.ico"
```

在 CMakeLists.txt 中添加：

```cmake
if(WIN32)
    target_sources(EnhanceVision PRIVATE resources/app.rc)
endif()
```

### 3.3 缩略图占位图加载

修改 `ThumbnailProvider::generatePlaceholderImage()` 方法：

```cpp
QImage ThumbnailProvider::loadPlaceholderFromResource()
{
    QImage placeholder;
    placeholder.load(":/icons/placeholder.png");
    
    if (placeholder.isNull()) {
        qWarning() << "[ThumbnailProvider] Failed to load placeholder from resource";
    }
    
    return placeholder;
}

QImage ThumbnailProvider::generatePlaceholderImage(const QSize& size)
{
    QSize placeholderSize = size.isValid() ? size : QSize(256, 256);
    
    QImage customPlaceholder = loadPlaceholderFromResource();
    
    if (!customPlaceholder.isNull()) {
        if (customPlaceholder.size() != placeholderSize) {
            return customPlaceholder.scaled(placeholderSize, 
                Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        }
        return customPlaceholder;
    }
    
    // 回退到默认灰色占位图
    // ...
}
```

### 3.4 QML 中的 Logo 显示

```qml
// TitleBar.qml
Image {
    source: "qrc:/icons/app_icon.png"
    sourceSize.width: 28
    sourceSize.height: 28
    fillMode: Image.PreserveAspectFit
    smooth: true
}

// MainPage.qml
Image {
    Layout.alignment: Qt.AlignHCenter
    source: "qrc:/icons/app_icon.png"
    sourceSize.width: 80
    sourceSize.height: 80
    fillMode: Image.PreserveAspectFit
    smooth: true
}
```

---

## 四、遇到的问题及解决方案

### 问题 1：Logo 需要圆角处理

**现象**：用户希望 Logo 图片有圆角边框效果。

**解决方案**：使用 Pillow 的 `ImageDraw.rounded_rectangle()` 方法创建圆角遮罩，应用到图片上。

### 问题 2：应用程序内部 Logo 未更新

**现象**：标题栏和欢迎界面的 Logo 仍显示为渐变矩形。

**解决方案**：修改 `TitleBar.qml` 和 `MainPage.qml`，将渐变矩形替换为 Image 组件加载自定义 Logo。

---

## 五、测试验证

| 测试场景 | 预期结果 | 实际结果 |
|----------|----------|----------|
| Windows 任务栏图标 | 显示自定义 Logo | ✅ 通过 |
| 窗口标题栏图标 | 显示自定义 Logo | ✅ 通过 |
| 标题栏左侧 Logo | 显示圆角 Logo | ✅ 通过 |
| 欢迎界面 Logo | 显示圆角 Logo | ✅ 通过 |
| 缩略图占位图 | 显示自定义占位图 | ✅ 通过 |

---

## 六、资源文件位置

| 资源 | 路径 | 用途 |
|------|------|------|
| 源 Logo | `tests/testAssetsDirectory/image/EnhanceVision.jpg` | 原始 Logo 文件 |
| 源占位图 | `tests/testAssetsDirectory/image/PlaceholderImage.png` | 原始占位图文件 |
| 应用图标 | `resources/icons/app_icon.png` | 应用程序图标 |
| Windows 图标 | `resources/icons/app_icon.ico` | Windows 可执行文件图标 |
| 缩略图占位图 | `resources/icons/placeholder.png` | 缩略图占位图 |

---

## 七、后续工作

- [ ] 考虑添加 macOS/Linux 应用程序图标支持
- [ ] 考虑添加高 DPI 图标支持
