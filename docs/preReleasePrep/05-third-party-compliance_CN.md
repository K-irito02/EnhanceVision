# 第三方依赖合规说明

[English](05-third-party-compliance_EN.md) | 简体中文

本文档详细说明 EnhanceVision 使用的第三方依赖及其许可证合规要求。

## 📋 目录

- [依赖概览](#依赖概览)
- [许可证兼容性分析](#许可证兼容性分析)
- [合规要求](#合规要求)
- [用户权利说明](#用户权利说明)
- [合规检查清单](#合规检查清单)

---

## 依赖概览

### 核心依赖

| 依赖 | 版本 | 许可证 | 使用方式 | 必需 |
|------|------|--------|----------|------|
| Qt | 6.10.2 | LGPL v3 | 动态链接 | ✅ |
| NCNN | latest | BSD-3-Clause | 静态链接 | ✅ |
| FFmpeg | 7.1 | LGPL v2.1+ | 动态链接 | ✅ |

### 可选依赖

| 依赖 | 版本 | 许可证 | 使用方式 | 必需 |
|------|------|--------|----------|------|
| OpenCV | 可选 | Apache 2.0 | 静态链接 | ❌ |
| Vulkan SDK | latest | Apache 2.0 | 动态链接 | ❌ |

### AI 模型

| 模型 | 来源 | 许可证 | 用途 |
|------|------|--------|------|
| Real-ESRGAN x4plus | xinntao/Real-ESRGAN | BSD-3-Clause | 图像超分辨率 |

---

## 许可证兼容性分析

### 许可证类型

```
MIT (本项目)
    │
    ├── ✅ BSD-3-Clause (NCNN, Real-ESRGAN)
    │       完全兼容，只需保留版权声明
    │
    ├── ✅ Apache 2.0 (OpenCV, Vulkan SDK)
    │       兼容，需保留版权声明和 NOTICE 文件
    │
    └── ✅ LGPL v2.1/v3 (Qt, FFmpeg)
            兼容，但需要动态链接
```

### 兼容性矩阵

| 许可证组合 | 兼容性 | 条件 |
|------------|--------|------|
| MIT + BSD-3 | ✅ | 保留版权声明 |
| MIT + Apache 2.0 | ✅ | 保留版权声明 |
| MIT + LGPL (动态链接) | ✅ | 动态链接，提供库源码获取方式 |
| MIT + LGPL (静态链接) | ⚠️ | 需提供目标文件或源码 |

---

## 合规要求

### Qt (LGPL v3)

#### 要求事项

1. **动态链接**
   - ✅ 已满足：本项目动态链接 Qt 库
   - 用户可替换 Qt 库版本

2. **提供源码获取方式**
   - 在文档中说明 Qt 源码下载地址
   - 在安装包中包含 LGPL 许可证文本

3. **版权声明**
   - 在 About 对话框中显示 Qt 版权信息
   - 在 THIRD_PARTY_LICENSES.md 中声明

#### 实施方案

**在应用程序中添加 About 对话框：**

```qml
// qml/pages/AboutPage.qml
Dialog {
    title: qsTr("About EnhanceVision")
    
    Column {
        Text {
            text: "EnhanceVision v0.1.0"
        }
        Text {
            text: qsTr("Built with Qt 6.10.2")
        }
        Text {
            text: qsTr("Qt is licensed under LGPL v3")
            link: "https://www.qt.io/licensing"
        }
    }
}
```

**在文档中声明：**

```markdown
## Qt Framework

本应用程序使用 Qt Framework，该框架根据 GNU Lesser General Public License v3.0 (LGPL v3) 授权。

- Qt 源代码可从 https://download.qt.io/official_releases/qt/ 获取
- LGPL v3 许可证全文可在 https://www.gnu.org/licenses/lgpl-3.0.html 获取
- 用户可以使用修改后的 Qt 库版本替换本应用程序附带的 Qt 库
```

### FFmpeg (LGPL v2.1+)

#### 要求事项

1. **使用 LGPL 版本**
   - ✅ 已满足：使用 FFmpeg LGPL 版本
   - 未使用 GPL 组件（如 libx264）

2. **动态链接**
   - ✅ 已满足：动态链接 FFmpeg 库

3. **提供源码获取方式**
   - 在文档中说明 FFmpeg 源码下载地址

#### 实施方案

**确认 FFmpeg 配置：**

```powershell
# 检查 FFmpeg 配置
ffmpeg -version

# 确认不包含 GPL 组件
# 输出应包含: --enable-lgpl
# 不应包含: --enable-gpl
```

**在文档中声明：**

```markdown
## FFmpeg

本应用程序使用 FFmpeg，该库根据 GNU Lesser General Public License v2.1 or later 授权。

- FFmpeg 源代码可从 https://ffmpeg.org/releases/ 获取
- LGPL v2.1 许可证全文可在 https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html 获取
```

### NCNN (BSD-3-Clause)

#### 要求事项

1. **保留版权声明**
   - 在 THIRD_PARTY_LICENSES.md 中包含完整许可证

2. **不使用项目名称背书**
   - 未经许可不得使用 "NCNN" 或 "Tencent" 进行推广

#### 实施方案

已在 THIRD_PARTY_LICENSES.md 中声明。

### Real-ESRGAN (BSD-3-Clause)

#### 要求事项

1. **保留版权声明**
   - 在模型文件说明中包含版权信息

2. **遵守使用限制**
   - 仅用于合法用途

#### 实施方案

**在模型目录添加 README：**

```markdown
# AI Models

本目录包含用于图像增强的 AI 模型。

## Real-ESRGAN x4plus

- 来源: https://github.com/xinntao/Real-ESRGAN
- 许可证: BSD 3-Clause License
- 版权: Copyright (c) 2021, Xintao Wang

使用本模型即表示您同意遵守其许可证条款。
```

---

## 用户权利说明

### 根据 LGPL 许可证，用户享有以下权利：

#### 1. 替换库文件的权利

用户可以使用修改后的 Qt 或 FFmpeg 库版本替换应用程序附带的库。

**替换步骤：**

1. 从官方源下载或自行编译 Qt/FFmpeg
2. 替换应用程序目录中的 DLL 文件
3. 重启应用程序

#### 2. 重新链接的权利

用户可以将应用程序与不同版本的库重新链接。

**技术说明：**
- 应用程序动态链接 Qt 和 FFmpeg
- 用户可以替换 DLL 文件实现重新链接
- 无需重新编译应用程序

#### 3. 获取源码的权利

用户有权获取 LGPL 许可库的源代码。

**获取方式：**
- Qt: https://download.qt.io/official_releases/qt/
- FFmpeg: https://ffmpeg.org/releases/

---

## 合规检查清单

### 发布前检查

- [ ] **Qt 合规**
  - [ ] 动态链接 Qt 库
  - [ ] 安装包包含 Qt LGPL 许可证文本
  - [ ] 文档说明 Qt 源码获取方式
  - [ ] About 对话框显示 Qt 版权信息

- [ ] **FFmpeg 合规**
  - [ ] 使用 LGPL 版本（非 GPL）
  - [ ] 动态链接 FFmpeg 库
  - [ ] 文档说明 FFmpeg 源码获取方式

- [ ] **NCNN 合规**
  - [ ] THIRD_PARTY_LICENSES.md 包含 BSD-3 许可证
  - [ ] 保留版权声明

- [ ] **Real-ESRGAN 合规**
  - [ ] 模型目录包含版权说明
  - [ ] THIRD_PARTY_LICENSES.md 包含许可证

### 安装包检查

- [ ] 包含 LICENSE 文件（MIT）
- [ ] 包含 THIRD_PARTY_LICENSES.md
- [ ] Qt DLL 文件为动态链接
- [ ] FFmpeg DLL 文件为动态链接
- [ ] 模型文件包含版权说明

### 文档检查

- [ ] README.md 包含许可证信息
- [ ] THIRD_PARTY_LICENSES.md 完整
- [ ] 关于对话框显示第三方版权

---

## 法律声明模板

### 应用程序启动画面/关于对话框

```
EnhanceVision v0.1.0
Copyright © 2025 EnhanceVision Contributors

本软件使用以下开源组件：
- Qt Framework (LGPL v3)
- NCNN (BSD-3-Clause)
- FFmpeg (LGPL v2.1+)

详见 THIRD_PARTY_LICENSES.md
```

### 安装程序许可协议页面

```
最终用户许可协议

EnhanceVision 根据 MIT 许可证授权。

本应用程序包含根据 LGPL 许可的第三方组件（Qt、FFmpeg）。
这些组件动态链接，您可以替换它们。

完整许可信息请参阅：
- LICENSE (MIT License)
- THIRD_PARTY_LICENSES.md (第三方许可证)

[我同意] [我不同意]
```

---

## 常见问题

### Q: 为什么选择 MIT 许可证？

A: MIT 是最宽松的开源许可证之一，允许用户自由使用、修改和分发代码，只需保留版权声明。这与我们的开源理念一致。

### Q: LGPL 许可证是否要求开源我的代码？

A: 不需要。只要动态链接 LGPL 库，您的应用程序代码可以保持闭源。用户只有权替换 LGPL 库本身。

### Q: 如果我想静态链接 Qt 怎么办？

A: 静态链接 LGPL 库需要：
1. 提供目标文件（.obj）以便用户重新链接
2. 或购买 Qt 商业许可证

我们选择动态链接以避免这些复杂性。

### Q: 用户如何替换 Qt 库？

A: 用户可以：
1. 下载或编译新版 Qt
2. 替换应用程序目录中的 Qt*.dll 文件
3. 重启应用程序

---

## 参考资料

- [Qt Licensing](https://www.qt.io/licensing)
- [FFmpeg License and Legal Considerations](https://ffmpeg.org/legal.html)
- [Choose a License](https://choosealicense.com/)
- [Open Source Initiative Licenses](https://opensource.org/licenses)

---

*最后更新: 2025年*
