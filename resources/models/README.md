# EnhanceVision AI 模型库

## 概述

本目录包含 EnhanceVision 项目所需的所有 AI 模型，涵盖图像/视频增强的多个领域。所有模型均采用 NCNN 格式，支持 Vulkan GPU 加速，可在 Intel/AMD/NVIDIA/Apple-Silicon GPU 上高效运行。

## 模型分类

| 类别 | 功能 | 已部署模型 |
|------|------|------------|
| [超分辨率](super_resolution/) | 图像/视频放大 | Real-ESRGAN, Waifu2x, SRMD |
| [去噪](denoising/) | 噪声去除 | SCUNet, NAFNet |
| [去模糊](deblurring/) | 模糊恢复 | MPRNet, Restormer |
| [去雾](dehazing/) | 雾霾去除 | FFA-Net |
| [上色](colorization/) | 黑白上色 | DeOldify |
| [低光增强](low_light/) | 低光恢复 | Zero-DCE++ |
| [视频插帧](frame_interpolation/) | 帧率提升 | RIFE |
| [图像修复](inpainting/) | 区域修复 | OpenCV Inpaint |

## 模型总览

### 超分辨率模型

| 模型 | 大小 | 格式 | 场景 | 论文 |
|------|------|------|------|------|
| Real-ESRGAN x4plus | 64 MB | NCNN | 通用照片 | ICCVW 2021 |
| Real-ESRGAN anime | 17 MB | NCNN | 动漫图片 | ICCVW 2021 |
| Real-ESRGAN animevideov3 | 3 MB | NCNN | 动漫视频 | ICCVW 2021 |
| Waifu2x CUNet | 3 MB | NCNN | 动漫图片 | SRCNN |
| Waifu2x UpConv7 | 1 MB | NCNN | 动漫快速 | SRCNN |
| SRMD | 3 MB | NCNN | 多退化 | CVPR 2018 |

### 去噪模型

| 模型 | 大小 | 格式 | 场景 | 论文 |
|------|------|------|------|------|
| SCUNet | 41 MB | NCNN | 真实去噪 | MIR 2023 |
| NAFNet | 222 MB | NCNN | 高质量去噪 | ECCV 2022 |

### 去模糊模型

| 模型 | 大小 | 格式 | 场景 | 论文 |
|------|------|------|------|------|
| MPRNet | 65 MB | NCNN | 运动去模糊 | CVPR 2021 |
| Restormer | 102 MB | ONNX | 多任务恢复 | CVPR 2022 Oral |

### 其他模型

| 模型 | 大小 | 格式 | 功能 | 论文 |
|------|------|------|------|------|
| FFA-Net | 8.5 MB | NCNN | 图像去雾 | AAAI 2020 |
| DeOldify | 243 MB | NCNN | 黑白上色 | - |
| Zero-DCE++ | 50 KB | NCNN | 低光增强 | TPAMI 2021 |
| RIFE | 10-57 MB | NCNN | 视频插帧 | ECCV 2022 |

## 目录结构

```
models/
├── models.json                    # 模型配置索引
├── README.md                      # 本文件
├── super_resolution/              # 超分辨率模型
│   ├── README.md
│   ├── Real-ESRGAN/
│   │   ├── README.md
│   │   ├── realesrgan-x4plus.param/bin
│   │   ├── realesrgan-x4plus-anime.param/bin
│   │   └── realesr-animevideov3-x*.param/bin
│   ├── Waifu2x/
│   │   ├── README.md
│   │   ├── models-cunet/
│   │   └── models-upconv_7_*/
│   └── SRMD/
│       ├── README.md
│       └── models-srmd/
├── denoising/                     # 去噪模型
│   ├── README.md
│   ├── SCUNet/
│   └── NAFNet/
├── deblurring/                    # 去模糊模型
│   ├── README.md
│   ├── MPRNet/
│   └── Restormer/
├── dehazing/                      # 去雾模型
│   ├── README.md
│   └── FFA-Net/
├── colorization/                  # 上色模型
│   ├── README.md
│   └── DeOldify/
├── low_light/                     # 低光增强模型
│   ├── README.md
│   └── Zero-DCE/
├── frame_interpolation/           # 视频插帧模型
│   ├── README.md
│   └── RIFE/
│       ├── rife-v4.6/
│       ├── rife-HD/
│       ├── rife-UHD/
│       └── rife-anime/
└── inpainting/                    # 图像修复模型
    ├── README.md
    └── OpenCV/
```

## 快速开始

### C++ (NCNN)

```cpp
#include "net.h"

ncnn::Net net;
net.opt.use_vulkan_compute = true;
net.load_param("model.param");
net.load_bin("model.bin");

ncnn::Mat in = ncnn::Mat::from_pixels(image_data, ncnn::Mat::PIXEL_RGB, w, h);

ncnn::Extractor ex = net.create_extractor();
ex.input("input", in);

ncnn::Mat out;
ex.extract("output", out);
```

### 命令行工具

```bash
# 超分辨率
realesrgan-ncnn-vulkan -i input.jpg -o output.png -n realesrgan-x4plus

# 视频插帧
rife-ncnn-vulkan -i input_frames/ -o output_frames/

# 动漫放大
waifu2x-ncnn-vulkan -i input.jpg -o output.png -n 2 -s 2
```

## 模型选择指南

### 按任务类型

| 任务 | 首选模型 | 备选模型 |
|------|----------|----------|
| 真实照片超分 | Real-ESRGAN x4plus | SRMD |
| 动漫图片超分 | Waifu2x CUNet | Real-ESRGAN anime |
| 动漫视频超分 | Real-ESRGAN animevideov3 | Waifu2x UpConv7 |
| 图像去噪 | SCUNet | NAFNet |
| 运动去模糊 | MPRNet | Restormer (ONNX) |
| 图像去雾 | FFA-Net | - |
| 黑白上色 | DeOldify | - |
| 低光增强 | Zero-DCE++ | - |
| 视频插帧 | RIFE v4.6 | RIFE HD/UHD |

### 按性能需求

| 需求 | 推荐模型 |
|------|----------|
| 最高质量 | Real-ESRGAN x4plus, NAFNet |
| 最快速度 | SRMD, Zero-DCE++, RIFE v4.6 |
| 最小模型 | Zero-DCE++ (50 KB), SRMD (3 MB) |
| 动漫最佳 | Waifu2x CUNet |
| 移动端部署 | Zero-DCE++, SRMD |

## 格式说明

### NCNN (.param / .bin)

- **优点**: 跨平台、Vulkan GPU 加速、无依赖
- **适用**: 所有主流模型
- **推理**: 使用 ncnn::Net 加载

### ONNX (.onnx)

- **优点**: 广泛支持、动态输入尺寸
- **适用**: Restormer (Transformer 架构)
- **推理**: 使用 ONNX Runtime

## 性能优化

### Vulkan GPU 加速

```cpp
ncnn::Net net;
net.opt.use_vulkan_compute = true;  // 启用 Vulkan
```

### 多 GPU 支持

```cpp
// 指定 GPU 设备
net.set_vulkan_device(0);  // 使用第一个 GPU
```

### 分块处理

对于大图像，使用分块处理避免显存不足：

```cpp
int tile_size = 512;  // 分块大小
// 分块处理逻辑...
```

## 模型来源

所有模型均来自官方开源项目，详见各子目录 README：

- [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN)
- [Waifu2x](https://github.com/nagadomi/waifu2x)
- [SRMD](https://github.com/cszn/SRMD)
- [SCUNet](https://github.com/cszn/SCUNet)
- [NAFNet](https://github.com/megvii-research/NAFNet)
- [MPRNet](https://github.com/swz30/MPRNet)
- [Restormer](https://github.com/swz30/Restormer)
- [FFA-Net](https://github.com/zhilin007/FFA-Net)
- [DeOldify](https://github.com/jantic/DeOldify)
- [Zero-DCE](https://github.com/Li-Chongyi/Zero-DCE_extension)
- [RIFE](https://github.com/hzwer/ECCV2022-RIFE)

## NCNN 移植项目

以下项目提供了高质量的 NCNN 移植：

- [Real-ESRGAN-ncnn-vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan)
- [waifu2x-ncnn-vulkan](https://github.com/nihui/waifu2x-ncnn-vulkan)
- [srmd-ncnn-vulkan](https://github.com/nihui/srmd-ncnn-vulkan)
- [rife-ncnn-vulkan](https://github.com/nihui/rife-ncnn-vulkan)

## 许可证

各模型遵循其原始项目的许可证，详见各模型目录的 README 文件。

---
*更新时间: 2026-03-21*
*维护者: EnhanceVision Team*
