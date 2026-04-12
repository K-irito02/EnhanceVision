# 拖拽覆盖层动画效果设计文档

## 概述

当用户拖拽多媒体文件到应用窗口时，会显示一个全屏覆盖层，包含艺术字提示文本和雨滴下落溅射动画效果。该设计旨在提供视觉上吸引人的反馈，同时保持功能性和响应性。

---

## 一、场景描述

### 1.1 触发条件
- 用户将文件从系统文件管理器拖拽到应用窗口上方
- 拖拽区域覆盖整个主页面

### 1.2 视觉效果概述

当拖拽开始时，整个窗口会被一个半透明的渐变背景覆盖，背景上有多条细长的"雨滴线"从顶部不断下落。每条雨滴线在到达底部时会产生溅射效果，向四周飞溅出多个小水滴。屏幕中央显示艺术字提示文本"释放以添加文件"（或英文"Release to add files"）。

### 1.3 动画元素

| 元素 | 描述 |
|------|------|
| **背景遮罩** | 渐变色背景，亮色/暗色主题自适应 |
| **雨滴线** | 15条细长的渐变色线条，从顶部下落 |
| **溅射水滴** | 雨滴落地时产生的小水珠，向四周飞溅 |
| **底部地面线** | 淡紫色发光线条，标识碰撞区域 |
| **艺术字文本** | 带发光、描边、阴影效果的提示文字 |

---

## 二、详细动画描述

### 2.1 雨滴下落动画

**外观特征：**
- 雨滴呈细长线条状，长度 60-140 像素
- 使用 Canvas 绘制，前端（底部）稍粗，后端（顶部）细
- 渐变色从顶部透明过渡到底部明亮白色
- 颜色渐变：透明 → 青色(#18CCFC) → 紫色(#6344F5) → 粉紫色(#AE48FF) → 白色

**运动特征：**
- 从屏幕顶部随机位置开始
- 初始延迟：0-2000ms（随机）
- 下落时间：1800-3300ms（随机，模拟不同速度）
- 使用 `Easing.InQuad` 缓动，模拟重力加速效果
- 到达底部地面线位置后消失

**循环机制：**
- 每条雨滴独立循环
- 消失后重置到顶部随机位置
- 重新随机化延迟和下落时间

### 2.2 溅射水滴动画

**触发条件：**
- 雨滴到达底部碰撞位置时触发

**外观特征：**
- 每次溅射产生 8-12 个小水滴
- 水滴大小：3-9 像素（随机）
- 圆形，带轻微模糊效果
- 颜色：青色、紫色、粉紫色、白色、天蓝色随机

**运动特征（慢动作效果，总时长约1.3秒）：**

1. **弹起阶段（500ms）**
   - 从碰撞点向上、向两侧飞溅
   - 水平扩散范围：±75 像素
   - 垂直上升高度：40-140 像素
   - 缩放从 0.3 放大到 1.0
   - 使用 `Easing.OutCubic` 缓动

2. **落下阶段（600ms）**
   - 受重力影响向下落
   - 下落距离：50-90 像素
   - 缩放从 1.0 缩小到 0.5
   - 使用 `Easing.InQuad` 缓动

3. **消失阶段（200ms）**
   - 透明度从 1 淡出到 0

### 2.3 艺术字效果

**字体选择：**
- 中文：白舟鯨海酔侯書体（日式书法风格）
- 英文：华文行楷

**字号计算：**
- 基于窗口尺寸动态计算
- 公式：`min(width, height) * 0.08`
- 范围限制：32px - 72px

**视觉层次（从底到顶）：**

1. **发光层**
   - 白色文字，50% 透明度
   - 高斯模糊效果（blur: 0.6）

2. **描边层**
   - 8个方向偏移的白色文字副本
   - 偏移距离：2 像素
   - 形成白色描边效果

3. **主文字层**
   - 黑色文字（#0A0A0A）
   - 带阴影效果
   - 阴影偏移：水平 1px，垂直 3px

### 2.4 入场/退场动画

**入场动画（并行执行）：**
- 背景遮罩：300ms 淡入
- 雨滴容器：350ms 淡入
- 文字容器：400ms 淡入
- 所有雨滴动画重置并启动

**退场动画（并行执行）：**
- 所有元素：200ms 淡出
- 动画停止并重置状态

---

## 三、技术实现

### 3.1 组件结构

```
DropOverlay.qml
├── FontLoader (chineseFont) - 白舟鯨海酔侯書体
├── FontLoader (englishFont) - 华文行楷
├── Rectangle (backdrop) - 背景遮罩
├── Item (beamsContainer) - 雨滴容器
│   ├── Rectangle (groundLine) - 底部地面线
│   ├── Repeater (beamRepeater) - 雨滴生成器 x15
│   │   └── Item (raindropItem)
│   │       ├── Canvas (raindropCanvas) - 雨滴绘制
│   │       └── SequentialAnimation (dropAnimation)
│   └── Item (splashContainer) - 溅射粒子容器
│       └── Repeater (splashRepeater) - 溅射粒子 x30
│           └── Item (splashItem)
│               ├── Rectangle (splashDrop)
│               └── SequentialAnimation (splashAnim)
├── Item (textContainer) - 艺术字容器
│   ├── Text (glowText) - 发光层
│   ├── Repeater - 描边层 x8
│   └── Text (artText) - 主文字层
├── ParallelAnimation (enterAnimation) - 入场动画
└── ParallelAnimation (exitAnimation) - 退场动画
```

### 3.2 关键技术点

#### 3.2.1 雨滴绘制（Canvas）

使用 QML Canvas 绘制前粗后细的雨滴形状：

```javascript
onPaint: {
    var ctx = getContext("2d")
    ctx.clearRect(0, 0, width, height)
    
    // 创建渐变
    var gradient = ctx.createLinearGradient(width/2, 0, width/2, height)
    gradient.addColorStop(0, "transparent")
    gradient.addColorStop(0.1, "rgba(24, 204, 252, 0.3)")
    gradient.addColorStop(0.3, "rgba(99, 68, 245, 0.7)")
    gradient.addColorStop(0.6, "rgba(174, 72, 255, 0.9)")
    gradient.addColorStop(0.85, "rgba(255, 255, 255, 1)")
    gradient.addColorStop(1, "rgba(255, 255, 255, 0.8)")
    
    // 绘制贝塞尔曲线形状
    ctx.beginPath()
    ctx.moveTo(width/2, 0)
    ctx.bezierCurveTo(width/2 - 0.3, height * 0.3, 
                      width/2 - 0.8, height * 0.7, 
                      width/2 - 1.2, height)
    ctx.arc(width/2, height, 1.2, Math.PI, 0, true)
    ctx.bezierCurveTo(width/2 + 0.8, height * 0.7,
                      width/2 + 0.3, height * 0.3,
                      width/2, 0)
    ctx.closePath()
    ctx.fillStyle = gradient
    ctx.fill()
}
```

#### 3.2.2 溅射粒子池

使用对象池模式管理溅射粒子，避免频繁创建/销毁：

```javascript
property int _splashIndex: 0

function _spawnSplash(collisionX, collisionY) {
    var particleCount = 8 + Math.floor(Math.random() * 5)
    for (var i = 0; i < particleCount; i++) {
        var p = splashRepeater.itemAt(_splashIndex)
        if (p) {
            p.startX = collisionX - p.width / 2
            p.startY = collisionY
            p.spreadX = (Math.random() - 0.5) * 150
            p.spreadY = -40 - Math.random() * 100
            p.fallY = 50 + Math.random() * 40
            p.animation.restart()
        }
        _splashIndex = (_splashIndex + 1) % splashRepeater.count
    }
}
```

#### 3.2.3 动画状态重置

确保每次拖拽都是全新开始：

```javascript
function _resetAllAnimations() {
    // 重置雨滴位置
    for (var i = 0; i < beamRepeater.count; i++) {
        var beam = beamRepeater.itemAt(i)
        if (beam) {
            beam.x = root.width * Math.random()
            beam.y = -beam.height - Math.random() * 300
            beam.opacity = 0
            beam.dropDelay = Math.random() * 2000
            beam.dropDuration = 1800 + Math.random() * 1500
        }
    }
    // 重置溅射粒子
    for (var j = 0; j < splashRepeater.count; j++) {
        var splash = splashRepeater.itemAt(j)
        if (splash) {
            splash.visible = false
            splash.animation.stop()
        }
    }
    _splashIndex = 0
}
```

#### 3.2.4 中英文字体自动切换

根据文本内容自动选择字体：

```javascript
property bool _isChineseText: /[\u4e00-\u9fa5]/.test(root.text)
property string _fontFamily: _isChineseText ? 
    (chineseFont.status === FontLoader.Ready ? chineseFont.name : "SimSun") :
    (englishFont.status === FontLoader.Ready ? englishFont.name : "Arial")
```

### 3.3 性能优化

1. **对象池**：溅射粒子使用固定数量的对象池（30个），循环复用
2. **层级裁剪**：雨滴容器启用 `clip: true`，防止超出边界的元素渲染
3. **条件渲染**：组件仅在 `active` 或 `_animating` 时可见
4. **模糊效果优化**：使用 `MultiEffect` 的 `blurMax` 限制模糊范围

---

## 四、文件清单

| 文件路径 | 描述 |
|----------|------|
| `qml/components/DropOverlay.qml` | 拖拽覆盖层组件实现 |
| `qml/pages/MainPage.qml` | 主页面，集成 DropOverlay |
| `resources/font/白舟鯨海酔侯書体.ttf` | 中文艺术字体 |
| `resources/font/华文行楷.ttf` | 英文艺术字体 |
| `CMakeLists.txt` | 构建配置，包含字体资源 |

---

## 五、使用方式

在 `MainPage.qml` 中的 `DropArea` 内使用：

```qml
DropArea {
    id: pageDropArea
    anchors.fill: parent
    
    DropOverlay {
        anchors.fill: parent
        active: pageDropArea.containsDrag
    }
    
    onDropped: (drop) => {
        // 处理文件
    }
}
```

---

## 六、设计灵感

本动画效果灵感来源于 [Aceternity UI](https://ui.aceternity.com/components/background-beams-with-collision) 的 "Background Beams With Collision" 组件，但针对 Qt/QML 平台进行了重新实现和优化，并加入了雨滴溅射的慢动作效果，使视觉体验更加流畅和自然。

---

**文档版本**: 1.0  
**创建日期**: 2026-04-13  
**最后更新**: 2026-04-13
