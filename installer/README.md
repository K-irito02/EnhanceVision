# 安装程序资源准备指南

本目录包含安装程序所需的资源文件。

## 目录结构

```
installer/
├── resources/           # 品牌图片资源
│   ├── welcome.bmp      # 欢迎页面图片 (500x314)
│   ├── header.bmp       # 页面头部图片 (150x57)
│   └── app.ico          # 应用图标 (256x256)
├── redist/              # 运行库安装程序
│   ├── vc_redist.x64.exe    # VC++ 2022 Redistributable (x64)
│   └── dxwebsetup.exe       # DirectX Web Setup (可选)
├── license/             # 许可协议文件
│   ├── license_zh.txt   # 中文许可协议
│   └── license_en.txt   # 英文许可协议
└── setup.nsi           # NSIS 主脚本
```

## 资源文件规格

### 1. 品牌图片

| 文件 | 尺寸 | 格式 | 说明 |
|------|------|------|------|
| welcome.bmp | 500 x 314 像素 | BMP 24-bit | 欢迎页面和完成页面背景图 |
| header.bmp | 150 x 57 像素 | BMP 24-bit | 安装页面顶部横幅 |

**设计建议：**
- 使用应用品牌色系
- 包含应用名称和 Logo
- 简洁大方，避免过多细节
- 支持深色/浅色主题（建议使用中性色调）

### 2. 应用图标

| 文件 | 尺寸 | 格式 | 说明 |
|------|------|------|------|
| app.ico | 256 x 256 像素 | ICO | 安装程序和卸载程序图标 |

**图标要求：**
- 包含多种尺寸 (16x16, 32x32, 48x48, 256x256)
- 透明背景
- 与应用主图标一致

### 3. 运行库

| 文件 | 下载地址 | 大小 |
|------|----------|------|
| vc_redist.x64.exe | https://aka.ms/vs/17/release/vc_redist.x64.exe | ~25MB |
| dxwebsetup.exe | https://www.microsoft.com/download/details.aspx?id=35 | ~300KB |

**下载命令：**
```powershell
# 下载 VC++ 运行库
Invoke-WebRequest -Uri "https://aka.ms/vs/17/release/vc_redist.x64.exe" -OutFile "installer\redist\vc_redist.x64.exe"

# 下载 DirectX Web Setup (可选)
Invoke-WebRequest -Uri "https://download.microsoft.com/download/1/7/1/1718CCC4-6EC5-465D-AEDC-6A1A2D66C6CF/dxwebsetup.exe" -OutFile "installer\redist\dxwebsetup.exe"
```

### 4. 许可协议文件

创建 `license/license_zh.txt`：
```
EnhanceVision 最终用户许可协议

版权所有 (c) 2025 EnhanceVision Contributors

特此免费授予任何获得本软件副本和相关文档文件（"软件"）的人不受限制地处置该软件的权利，
包括不受限制地使用、复制、修改、合并、发布、分发、再许可和/或销售该软件副本的权利，
以及再授权被配发了本软件的人如上的权利，须在下列条件下：

上述版权声明和本许可声明应包含在该软件的所有副本或实质成分中。

本软件按"原样"提供，不提供任何形式的担保，无论是明示的还是暗示的，
包括但不限于适销性、特定用途适用性和非侵权性的担保。在任何情况下，
作者或版权持有人均不对任何索赔、损害或其他责任负责，
无论是在合同诉讼、侵权行为还是其他方面，由软件或软件的使用或其他交易引起、
由此产生或与之相关。
```

创建 `license/license_en.txt`：
```
EnhanceVision End User License Agreement

Copyright (c) 2025 EnhanceVision Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 图片制作工具推荐

### 在线工具
- Canva (https://www.canva.com/) - 免费在线设计工具
- Figma (https://www.figma.com/) - 专业 UI 设计工具
- Photopea (https://www.photopea.com/) - 在线图片编辑器

### 本地工具
- GIMP (https://www.gimp.org/) - 免费开源图片编辑器
- Paint.NET (https://www.getpaint.net/) - Windows 免费图片编辑器

## 图片转换命令

如果需要将 PNG 转换为 BMP：

```powershell
# 使用 PowerShell (需要安装 ImageMagick)
magick convert welcome.png -resize 500x314 -depth 24 BMP3:welcome.bmp
magick convert header.png -resize 150x57 -depth 24 BMP3:header.bmp
```

## 检查清单

在运行打包脚本前，请确认：

- [ ] `resources/welcome.bmp` 存在且尺寸正确 (500x314)
- [ ] `resources/header.bmp` 存在且尺寸正确 (150x57)
- [ ] `resources/app.ico` 存在
- [ ] `redist/vc_redist.x64.exe` 存在
- [ ] `license/license_zh.txt` 存在
- [ ] `license/license_en.txt` 存在
- [ ] `setup.nsi` 脚本已创建

## 注意事项

1. **图片格式**：NSIS 要求 BMP 为 24-bit 格式，不支持 32-bit (带 Alpha 通道)
2. **图片尺寸**：尺寸不匹配会导致显示异常
3. **运行库版本**：建议使用最新版本的 VC++ Redistributable
4. **许可协议**：确保与项目 LICENSE 文件内容一致
