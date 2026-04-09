# 第三方许可证声明

EnhanceVision 使用了以下第三方开源软件库。我们感谢这些项目的贡献者。

## Qt Framework

**许可证**: GNU Lesser General Public License v3.0 (LGPL v3)

**项目地址**: https://www.qt.io/

**版权声明**:
```
Copyright (C) The Qt Company Ltd. and other contributors.
```

**使用方式**: 动态链接

**LGPL v3 合规说明**:
- 本应用程序动态链接 Qt 库，未对 Qt 源代码进行修改
- 用户可以使用修改后的 Qt 库版本替换本应用程序附带的 Qt 库
- Qt 源代码可从 https://download.qt.io/official_releases/qt/ 获取
- 完整的 LGPL v3 许可证文本可在 https://www.gnu.org/licenses/lgpl-3.0.html 获取

---

## NCNN

**许可证**: BSD 3-Clause License

**项目地址**: https://github.com/Tencent/ncnn

**版权声明**:
```
Copyright (c) 2017 THL A29 Limited, a Tencent company. All rights reserved.
```

**许可证全文**:
```
BSD 3-Clause License

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## FFmpeg

**许可证**: GNU Lesser General Public License v2.1 or later (LGPL v2.1+)

**项目地址**: https://ffmpeg.org/

**版权声明**:
```
Copyright (c) FFmpeg developers
```

**使用方式**: 动态链接

**LGPL 合规说明**:
- 本应用程序使用 FFmpeg 的 LGPL 版本
- FFmpeg 库以动态链接方式使用
- 用户可以使用修改后的 FFmpeg 库版本替换本应用程序附带的 FFmpeg 库
- FFmpeg 源代码可从 https://ffmpeg.org/releases/ 获取
- 完整的 LGPL v2.1 许可证文本可在 https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html 获取

---

## OpenCV (可选依赖)

**许可证**: Apache License 2.0

**项目地址**: https://opencv.org/

**版权声明**:
```
Copyright (C) 2000-2024, Intel Corporation, all rights reserved.
Copyright (C) 2009-2011, Willow Garage Inc., all rights reserved.
Copyright (C) 2009-2016, NVIDIA Corporation, all rights reserved.
Copyright (C) 2010-2013, Advanced Micro Devices, Inc., all rights reserved.
Copyright (C) 2015-2016, OpenCV Foundation, all rights reserved.
Copyright (C) 2015-2024, OpenCV team, all rights reserved.
```

**许可证全文**:
```
Apache License
Version 2.0, January 2004
http://www.apache.org/licenses/

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

---

## Vulkan SDK

**许可证**: Apache License 2.0

**项目地址**: https://vulkan.lunarg.com/

**版权声明**:
```
Copyright (c) 2015-2024 The Khronos Group Inc.
Copyright (c) 2015-2024 LunarG, Inc.
```

---

## Real-ESRGAN 模型

**许可证**: BSD 3-Clause License

**项目地址**: https://github.com/xinntao/Real-ESRGAN

**版权声明**:
```
Copyright (c) 2021, Xintao Wang
```

---

## 许可证兼容性说明

| 组件 | 许可证 | 与 MIT 兼容 | 备注 |
|------|--------|-------------|------|
| Qt 6 | LGPL v3 | ✅ | 动态链接 |
| NCNN | BSD-3-Clause | ✅ | 完全兼容 |
| FFmpeg | LGPL v2.1+ | ✅ | 动态链接 |
| OpenCV | Apache 2.0 | ✅ | 完全兼容 |
| Vulkan SDK | Apache 2.0 | ✅ | 完全兼容 |
| Real-ESRGAN | BSD-3-Clause | ✅ | 完全兼容 |

---

## 用户权利

根据上述许可证，用户享有以下权利：

1. **使用**: 可以自由使用本软件
2. **研究**: 可以研究软件的工作原理
3. **修改**: 可以修改软件以适应自己的需求
4. **分发**: 可以分发软件的副本

对于 LGPL 许可的组件（Qt、FFmpeg），用户还享有：
- 用修改后的库版本替换应用程序附带的库版本的权利
- 将应用程序与不同版本的库重新链接的权利

---

## 获取源代码

- **Qt**: https://download.qt.io/official_releases/qt/
- **NCNN**: https://github.com/Tencent/ncnn
- **FFmpeg**: https://ffmpeg.org/releases/
- **OpenCV**: https://github.com/opencv/opencv
- **Vulkan SDK**: https://vulkan.lunarg.com/sdk/home
- **Real-ESRGAN**: https://github.com/xinntao/Real-ESRGAN

---

*最后更新: 2025年*
