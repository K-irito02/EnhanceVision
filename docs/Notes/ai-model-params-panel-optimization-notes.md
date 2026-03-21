# AI 模型参数面板优化笔记

## 概述

优化 AI 模型参数配置面板，实现本地化支持、控件状态平滑过渡、专业术语通俗化解释等功能。

**创建日期**: 2026-03-21
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `qml/components/AIParamsPanel.qml` | 添加本地化函数、平滑过渡动画、语言切换监听 |
| `resources/models/models.json` | 完善所有模型的 supportedParams 配置，添加通俗化描述 |

### 2. 实现的功能特性

- ✅ 参数标签本地化支持（中英文切换）
- ✅ 参数描述本地化支持
- ✅ 控件状态平滑过渡动画
- ✅ 专业术语通俗化解释
- ✅ 深拷贝参数对象避免引用问题

---

## 二、遇到的问题及解决方案

### 问题 1：本地化支持不足

**现象**：参数标签仅显示单一语言，无法根据语言设置切换

**原因**：AIParamsPanel 使用 `modelData.name` 而非 `label`/`label_en`，未根据 `Theme.language` 选择语言

**解决方案**：
```qml
function localizedText(param, zhField, enField) {
    if (!param) return ""
    var isEnglish = Theme.language === "en_US"
    if (isEnglish && param[enField] !== undefined && param[enField] !== "") {
        return param[enField]
    }
    return param[zhField] !== undefined ? param[zhField] : ""
}
```

### 问题 2：控件状态跳动

**现象**：模型切换时控件出现闪烁、位置偏移或数值突变

**原因**：Repeater 在模型切换时立即重建控件，导致视觉跳动

**解决方案**：
- 添加 `_isModelSwitching` 状态标志
- 为 Slider 添加 `Behavior on value` 动画
- 使用 `JSON.parse(JSON.stringify())` 深拷贝参数对象

### 问题 3：专业术语难以理解

**现象**：TTA 模式等专业术语用户难以理解

**原因**：描述过于技术化，缺少通俗解释

**解决方案**：为所有参数添加通俗易懂的中英文描述

---

## 三、技术实现细节

### 本地化辅助函数

```qml
function getParamLabel(param) {
    return localizedText(param, "label", "label_en") || param.key || ""
}

function getParamDescription(param) {
    return localizedText(param, "description", "description_en")
}
```

### 平滑过渡动画

```qml
Behavior on value {
    enabled: !sliderControl.pressed
    NumberAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
}
```

### 语言切换监听

```qml
Connections {
    target: Theme
    function onLanguageChanged() {
        paramsRepeater.model = paramsRepeater.model
    }
}
```

---

## 四、参数描述优化示例

| 参数名称 | 原描述 | 新描述（中文） |
|---------|--------|---------------|
| TTA 模式 | 测试时增强，提高质量但降低速度 | 通过多次处理并合并结果来提升画质，但处理时间会增加约8倍 |
| 噪声级别 | 去噪强度，-1表示无去噪 | 去除噪点的强度，-1表示不去噪，数值越大去噪越强 |
| 渲染因子 | 较高的值产生更详细的结果但速度较慢 | 数值越大颜色越丰富细腻，但处理速度会变慢。推荐35 |

---

## 五、模型参数配置统计

| 模型系列 | 参数数量 | 主要参数 |
|---------|---------|---------|
| Real-ESRGAN x4plus | 2 | outscale, face_enhance |
| Real-ESRGAN Anime | 1 | outscale |
| RealESR AnimeVideo v3 | 1 | outscale |
| SRMD | 2 | noise_level, tta_mode |
| Waifu2x | 1 | tta_mode |
| RIFE v4.6 | 4 | time_step, uhd_mode, tta_spatial, tta_temporal |
| DeOldify | 2 | render_factor, artistic_mode |
| Zero-DCE | 2 | enhancement_strength, exposure_correction |

---

## 六、参考资料

- [Qt Quick Controls 2 文档](https://doc.qt.io/qt-6/qtquickcontrols2-index.html)
- [QML Behaviors](https://doc.qt.io/qt-6/qml-qtquick-behavior.html)
