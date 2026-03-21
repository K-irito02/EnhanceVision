# EnhanceVision 推理引擎与模型选型最终方案

> **文档类型**：最终技术方案  
> **适用项目**：EnhanceVision  
> **技术路线**：纯 NCNN 本地离线推理  
> **创建时间**：2026-03-21  
> **版本**：v1.0

---

## 一、技术方案概述

### 1.1 核心决策

| 决策项 | 选择 | 说明 |
|--------|------|------|
| **推理引擎** | NCNN | 唯一推理引擎，跨平台、轻量、Vulkan GPU 加速 |
| **模型格式** | .param / .bin | NCNN 原生格式 |
| **转换策略** | PyTorch → ONNX → NCNN | 统一转换流程 |
| **部署模式** | 纯本地离线 | 无需网络，无云端依赖 |
| **GPU 加速** | Vulkan | 跨平台 GPU 加速方案 |

### 1.2 技术架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                    EnhanceVision 推理架构                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                      应用层 (Qt/QML)                          │  │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐          │  │
│  │  │ 超分辨率 │  │  去噪   │  │ 去模糊  │  │ 低光增强 │  ...     │  │
│  │  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘          │  │
│  └───────┼────────────┼────────────┼────────────┼────────────────┘  │
│          │            │            │            │                   │
│  ┌───────┴────────────┴────────────┴────────────┴────────────────┐  │
│  │                   统一推理接口层 (AIEngine)                    │  │
│  │  ┌─────────────────────────────────────────────────────────┐  │  │
│  │  │  • loadModel()     • process()      • getModelInfo()   │  │  │
│  │  │  • setParameters() • cancelProcess() • getProgress()   │  │  │
│  │  └─────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                  │                                  │
│  ┌───────────────────────────────┴───────────────────────────────┐  │
│  │                    NCNN 推理引擎层                             │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐            │  │
│  │  │ ncnn::Net   │  │ ncnn::Mat   │  │ VulkanDevice │            │  │
│  │  │ 模型加载    │  │ 数据处理    │  │  GPU 加速    │            │  │
│  │  └─────────────┘  └─────────────┘  └─────────────┘            │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                  │                                  │
│  ┌───────────────────────────────┴───────────────────────────────┐  │
│  │                      模型文件层                                │  │
│  │                                                               │  │
│  │   resources/models/                                           │  │
│  │   ├── super_resolution/  (超分辨率模型)                        │  │
│  │   ├── denoising/         (去噪模型)                            │  │
│  │   ├── deblurring/        (去模糊模型)                          │  │
│  │   ├── dehazing/          (去雾模型)                            │  │
│  │   ├── colorization/      (上色模型)                            │  │
│  │   ├── inpainting/        (图像修复模型)                        │  │
│  │   ├── low_light/         (低光增强模型)                        │  │
│  │   └── frame_interp/      (视频插帧模型)                        │  │
│  │                                                               │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 二、支持模型完整列表

### 2.1 超分辨率模型（通用）- 14 个

| 序号 | 模型名称 | 放大倍数 | 模型大小 | 部署方式 | 优先级 |
|------|----------|----------|----------|----------|--------|
| 1 | **Real-ESRGAN x4plus** | 4x | 64MB | 官方 NCNN | ⭐⭐⭐ 核心 |
| 2 | **Real-ESRGAN x2plus** | 2x | 64MB | 官方 NCNN | ⭐⭐⭐ 核心 |
| 3 | **RealESRNet x4plus** | 4x | 64MB | 官方 NCNN | ⭐⭐ 推荐 |
| 4 | **ESRGAN** | 4x | 64MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 5 | **BSRGAN** | 4x | 64MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 6 | **EDSR** | 2x/3x/4x | 40-170MB | ONNX→NCNN | ⭐ 可选 |
| 7 | **RCAN** | 2x/4x | 60MB | ONNX→NCNN | ⭐ 可选 |
| 8 | **SRResNet** | 4x | 6MB | ONNX→NCNN | ⭐⭐ 轻量 |
| 9 | **SRCNN** | 2x/3x/4x | 0.2MB | ONNX→NCNN | ⭐ 极轻量 |
| 10 | **VDSR** | 2x/3x/4x | 2.7MB | ONNX→NCNN | ⭐ 轻量 |
| 11 | **CARN** | 2x/4x | 6MB | ONNX→NCNN | ⭐⭐ 轻量 |
| 12 | **IMDN** | 4x | 0.7MB | ONNX→NCNN | ⭐⭐ 极轻量 |
| 13 | **PAN** | 4x | 0.3MB | ONNX→NCNN | ⭐⭐ 超轻量 |
| 14 | **RealSR** | 4x | 64MB | ONNX→NCNN | ⭐ 可选 |

### 2.2 超分辨率模型（动漫）- 8 个

| 序号 | 模型名称 | 放大倍数 | 模型大小 | 部署方式 | 优先级 |
|------|----------|----------|----------|----------|--------|
| 1 | **Real-ESRGAN Anime** | 4x | 17MB | 官方 NCNN | ⭐⭐⭐ 核心 |
| 2 | **Real-ESRGAN AnimeVideo v3** | 4x | 3MB | 官方 NCNN | ⭐⭐⭐ 核心 |
| 3 | **Waifu2x CUNet** | 2x | 3MB | 官方 NCNN | ⭐⭐⭐ 核心 |
| 4 | **Waifu2x UpConv** | 2x | 1MB | 官方 NCNN | ⭐⭐ 推荐 |
| 5 | **Real-CUGAN** | 2x/3x/4x | 1-7MB | 官方 NCNN | ⭐⭐⭐ 核心 |
| 6 | **Real-CUGAN Pro** | 2x/3x | 2-10MB | 官方 NCNN | ⭐⭐⭐ 核心 |
| 7 | **ACNet** | 2x | 1MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 8 | **SRMD** | 2x/3x/4x | 6MB | 官方 NCNN | ⭐⭐ 推荐 |

### 2.3 图像去噪模型 - 11 个

| 序号 | 模型名称 | 模型大小 | 部署方式 | 优先级 |
|------|----------|----------|----------|--------|
| 1 | **DnCNN** | 0.7MB | ONNX→NCNN | ⭐⭐ 经典 |
| 2 | **FFDNet** | 0.9MB | ONNX→NCNN | ⭐⭐ 灵活 |
| 3 | **SCUNet** | 9MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 4 | **SCUNet-GAN** | 9MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 5 | **NAFNet** | 8-67MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 6 | **CBDNet** | 16MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 7 | **RIDNet** | 6MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 8 | **MIRNet** | 31MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 9 | **MIRNet-v2** | 6MB | ONNX→NCNN | ⭐⭐ 轻量 |
| 10 | **MPRNet** | 20MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 11 | **VDN** | 8MB | ONNX→NCNN | ⭐ 可选 |

### 2.4 图像去模糊模型 - 6 个

| 序号 | 模型名称 | 模型大小 | 部署方式 | 优先级 |
|------|----------|----------|----------|--------|
| 1 | **DeblurGAN** | 11MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 2 | **DeblurGAN-v2** | 15-60MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 3 | **MIMO-UNet** | 6MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 4 | **MIMO-UNet+** | 16MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 5 | **NAFNet-Deblur** | 8-67MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 6 | **HINet** | 89MB | ONNX→NCNN | ⭐ 可选 |

### 2.5 图像去雾模型 - 6 个

| 序号 | 模型名称 | 模型大小 | 部署方式 | 优先级 |
|------|----------|----------|----------|--------|
| 1 | **AOD-Net** | 0.1MB | ONNX→NCNN | ⭐⭐⭐ 超轻量 |
| 2 | **DehazeNet** | 0.3MB | ONNX→NCNN | ⭐⭐ 经典 |
| 3 | **FFA-Net** | 4MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 4 | **MSBDN-DFF** | 31MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 5 | **GridDehazeNet** | 1MB | ONNX→NCNN | ⭐⭐ 轻量 |
| 6 | **AECR-Net** | 2.6MB | ONNX→NCNN | ⭐⭐ 推荐 |

### 2.6 图像上色模型 - 2 个

| 序号 | 模型名称 | 模型大小 | 部署方式 | 优先级 |
|------|----------|----------|----------|--------|
| 1 | **Colorization (Zhang)** | 129MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 2 | **ChromaGAN** | 70MB | ONNX→NCNN | ⭐⭐ 推荐 |

### 2.7 图像修复模型 - 6 个

| 序号 | 模型名称 | 模型大小 | 部署方式 | 优先级 |
|------|----------|----------|----------|--------|
| 1 | **LaMa** | 51MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 2 | **LaMa-Fourier** | 51MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 3 | **AOT-GAN** | 12MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 4 | **DeepFill v2** | 20MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 5 | **EdgeConnect** | 15MB | ONNX→NCNN | ⭐ 可选 |
| 6 | **PartialConv** | 15MB | ONNX→NCNN | ⭐ 可选 |

### 2.8 低光照增强模型 - 9 个

| 序号 | 模型名称 | 模型大小 | 部署方式 | 优先级 |
|------|----------|----------|----------|--------|
| 1 | **Zero-DCE** | 0.3MB | ONNX→NCNN | ⭐⭐⭐ 核心 |
| 2 | **Zero-DCE++** | 0.01MB | ONNX→NCNN | ⭐⭐⭐ 超轻量 |
| 3 | **EnlightenGAN** | 33MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 4 | **RetinexNet** | 2MB | ONNX→NCNN | ⭐⭐ 经典 |
| 5 | **KinD** | 8MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 6 | **KinD++** | 8MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 7 | **SCI** | 0.01MB | ONNX→NCNN | ⭐⭐⭐ 超轻量 |
| 8 | **URetinex-Net** | 1MB | ONNX→NCNN | ⭐⭐ 轻量 |
| 9 | **RUAS** | 0.003MB | ONNX→NCNN | ⭐⭐ 极轻量 |

### 2.9 视频插帧模型 - 4 个

| 序号 | 模型名称 | 模型大小 | 部署方式 | 优先级 |
|------|----------|----------|----------|--------|
| 1 | **RIFE** | 7-15MB | 官方 NCNN | ⭐⭐⭐ 核心 |
| 2 | **RIFE v4.6** | 15MB | 官方 NCNN | ⭐⭐⭐ 核心 |
| 3 | **IFRNet** | 20MB | ONNX→NCNN | ⭐⭐ 推荐 |
| 4 | **CAIN** | 40MB | ONNX→NCNN | ⭐ 可选 |

### 2.10 模型统计汇总

| 类别 | 数量 | 总大小（最小配置） | 核心模型数 |
|------|------|-------------------|------------|
| 超分辨率（通用） | 14 | ~400MB | 3 |
| 超分辨率（动漫） | 8 | ~35MB | 6 |
| 去噪 | 11 | ~120MB | 3 |
| 去模糊 | 6 | ~130MB | 4 |
| 去雾 | 6 | ~40MB | 2 |
| 上色 | 2 | ~200MB | 1 |
| 图像修复 | 6 | ~165MB | 1 |
| 低光增强 | 9 | ~55MB | 3 |
| 视频插帧 | 4 | ~90MB | 2 |
| **合计** | **66** | **~1.2GB** | **25** |

---

## 三、模型部署详细方案

### 3.1 官方 NCNN 模型（直接使用）

以下模型官方已提供 NCNN 格式，可直接下载使用：

#### Real-ESRGAN 系列
```bash
# 下载地址
https://github.com/xinntao/Real-ESRGAN/releases

# 可用模型
- realesrgan-x4plus.param / .bin          (64MB, 通用4x)
- realesrgan-x4plus-anime.param / .bin    (17MB, 动漫4x)
- realesr-animevideov3.param / .bin       (3MB, 动漫视频4x)
```

#### Real-CUGAN 系列（注：这是 Waifu2x 系列的早期版本，我直接使用 Waifu2x 系列）
```bash
# 下载地址
https://github.com/nihui/realcugan-ncnn-vulkan/releases

# 可用模型 (models-pro/)
- up2x-conservative.param / .bin   (2x 保守)
- up2x-no-denoise.param / .bin     (2x 无降噪)
- up2x-denoise1x.param / .bin      (2x 轻度降噪)
- up2x-denoise2x.param / .bin      (2x 中度降噪)
- up2x-denoise3x.param / .bin      (2x 强降噪)
- up3x-conservative.param / .bin   (3x)
- up4x-conservative.param / .bin   (4x)
```

#### Waifu2x 系列
```bash
# 下载地址
https://github.com/nihui/waifu2x-ncnn-vulkan/releases

# 可用模型
- models-cunet/           (CUNet 架构)
- models-upconv_7/        (UpConv 架构)
```

#### RIFE 系列
```bash
# 下载地址
https://github.com/nihui/rife-ncnn-vulkan/releases

# 可用模型
- rife-v4.6/             (最新稳定版)
- rife-v4/               (v4.0)
```

#### SRMD
```bash
# 下载地址
https://github.com/nihui/srmd-ncnn-vulkan/releases
```

### 3.2 需要转换的模型（ONNX → NCNN）

#### 转换工具准备

```bash
# 1. 安装 Python 依赖
pip install torch torchvision onnx onnxsim

# 2. 下载 PNNX 工具
# https://github.com/pnnx/pnnx/releases
# 选择对应平台的二进制文件

# 3. 下载 onnx2ncnn（备用）
# 包含在 ncnn 编译产物中
```

#### 通用转换流程

```bash
# 步骤 1: PyTorch → ONNX
python -c "
import torch
from model import MyModel

model = MyModel()
model.load_state_dict(torch.load('model.pth'))
model.eval()

dummy_input = torch.randn(1, 3, 256, 256)
torch.onnx.export(
    model, dummy_input, 'model.onnx',
    opset_version=12,
    input_names=['input'],
    output_names=['output'],
    dynamic_axes={'input': {2: 'h', 3: 'w'}, 'output': {2: 'h', 3: 'w'}}
)
"

# 步骤 2: 简化 ONNX
python -m onnxsim model.onnx model_sim.onnx

# 步骤 3: ONNX → NCNN (推荐 PNNX)
./pnnx model_sim.onnx inputshape=[1,3,256,256]

# 或使用 onnx2ncnn (备用)
./onnx2ncnn model_sim.onnx model.param model.bin
```

### 3.3 各模型具体转换指南

#### SCUNet 转换
```bash
# 1. 克隆仓库
git clone https://github.com/cszn/SCUNet.git
cd SCUNet

# 2. 下载预训练权重
wget https://github.com/cszn/KAIR/releases/download/v1.0/scunet_color_real_psnr.pth

# 3. 导出 ONNX
python -c "
import torch
from models.network_scunet import SCUNet

model = SCUNet(in_nc=3, config=[4,4,4,4,4,4,4], dim=64)
model.load_state_dict(torch.load('scunet_color_real_psnr.pth'), strict=True)
model.eval()

x = torch.randn(1, 3, 256, 256)
torch.onnx.export(model, x, 'scunet.onnx', opset_version=12,
                  input_names=['input'], output_names=['output'])
"

# 4. 转换为 NCNN
python -m onnxsim scunet.onnx scunet_sim.onnx
./pnnx scunet_sim.onnx inputshape=[1,3,256,256]
```

#### NAFNet 转换
```bash
# 1. 克隆仓库
git clone https://github.com/megvii-research/NAFNet.git
cd NAFNet

# 2. 安装依赖
pip install -r requirements.txt

# 3. 下载预训练权重
# 从 https://github.com/megvii-research/NAFNet/releases 下载

# 4. 导出 ONNX
python -c "
import torch
from basicsr.models.archs.NAFNet_arch import NAFNet

model = NAFNet(img_channel=3, width=32, middle_blk_num=1,
               enc_blk_nums=[1,1,1,28], dec_blk_nums=[1,1,1,1])
model.load_state_dict(torch.load('NAFNet-width32.pth')['params'], strict=True)
model.eval()

x = torch.randn(1, 3, 256, 256)
torch.onnx.export(model, x, 'nafnet.onnx', opset_version=12)
"

# 5. 转换
python -m onnxsim nafnet.onnx nafnet_sim.onnx
./pnnx nafnet_sim.onnx inputshape=[1,3,256,256]
```

#### LaMa 转换
```bash
# 1. 克隆仓库
git clone https://github.com/advimman/lama.git
cd lama

# 2. 导出 ONNX（需要修改部分代码处理掩码输入）
# LaMa 需要图像和掩码两个输入
python -c "
import torch
from saicinpainting.training.trainers import load_checkpoint

model = load_checkpoint(train_config, 'big-lama.ckpt', strict=False)
model.eval()

image = torch.randn(1, 3, 512, 512)
mask = torch.randn(1, 1, 512, 512)
torch.onnx.export(model, (image, mask), 'lama.onnx', opset_version=12,
                  input_names=['image', 'mask'], output_names=['output'])
"

# 3. 转换
python -m onnxsim lama.onnx lama_sim.onnx
./pnnx lama_sim.onnx inputshape=[1,3,512,512],[1,1,512,512]
```

#### Zero-DCE++ 转换
```bash
# 1. 克隆仓库
git clone https://github.com/Li-Chongyi/Zero-DCE_extension.git
cd Zero-DCE_extension

# 2. 导出 ONNX
python -c "
import torch
from model import enhance_net_nopool

model = enhance_net_nopool()
model.load_state_dict(torch.load('snapshots/Epoch99.pth'))
model.eval()

x = torch.randn(1, 3, 256, 256)
torch.onnx.export(model, x, 'zerodce_pp.onnx', opset_version=12)
"

# 3. 转换
python -m onnxsim zerodce_pp.onnx zerodce_pp_sim.onnx
./pnnx zerodce_pp_sim.onnx inputshape=[1,3,256,256]
```

---

## 四、推理引擎实现

### 4.1 核心类设计

```cpp
// include/EnhanceVision/core/AIEngine.h

#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QVariantMap>
#include <memory>
#include <net.h>

namespace EnhanceVision {

enum class ModelCategory {
    SuperResolution,      // 超分辨率
    SuperResolutionAnime, // 动漫超分辨率
    Denoising,           // 去噪
    Deblurring,          // 去模糊
    Dehazing,            // 去雾
    Colorization,        // 上色
    Inpainting,          // 图像修复
    LowLightEnhance,     // 低光增强
    FrameInterpolation   // 视频插帧
};

struct ModelInfo {
    QString id;
    QString name;
    QString description;
    ModelCategory category;
    QString paramPath;
    QString binPath;
    qint64 sizeBytes;
    int scaleFactor;      // 放大倍数（超分专用）
    bool isLoaded;
    QVariantMap parameters;
};

class AIEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    explicit AIEngine(QObject *parent = nullptr);
    ~AIEngine();

    // 模型管理
    Q_INVOKABLE bool loadModel(const QString &modelId);
    Q_INVOKABLE void unloadModel();
    Q_INVOKABLE QVariantList getAvailableModels(ModelCategory category);
    Q_INVOKABLE QVariantMap getModelInfo(const QString &modelId);
    
    // 推理接口
    Q_INVOKABLE QImage process(const QImage &input);
    Q_INVOKABLE void processAsync(const QImage &input);
    Q_INVOKABLE void cancelProcess();
    
    // 参数设置
    Q_INVOKABLE void setParameter(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant getParameter(const QString &name) const;
    
    // 状态查询
    bool isProcessing() const { return m_isProcessing; }
    double progress() const { return m_progress; }
    QString currentModelId() const { return m_currentModelId; }

signals:
    void modelLoaded(const QString &modelId);
    void modelUnloaded();
    void processingChanged(bool processing);
    void progressChanged(double progress);
    void processCompleted(const QImage &result);
    void processError(const QString &error);

private:
    void initVulkan();
    void loadModelRegistry();
    ncnn::Mat qimageToMat(const QImage &image);
    QImage matToQimage(const ncnn::Mat &mat);
    
    ncnn::Net m_net;
    ncnn::VulkanDevice *m_vkdev = nullptr;
    ncnn::Option m_opt;
    
    QString m_currentModelId;
    ModelInfo m_currentModel;
    QMap<QString, ModelInfo> m_modelRegistry;
    
    bool m_isProcessing = false;
    double m_progress = 0.0;
    bool m_cancelRequested = false;
};

} // namespace EnhanceVision
```

### 4.2 实现文件

```cpp
// src/core/AIEngine.cpp

#include "EnhanceVision/core/AIEngine.h"
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <gpu.h>

namespace EnhanceVision {

AIEngine::AIEngine(QObject *parent)
    : QObject(parent)
{
    initVulkan();
    loadModelRegistry();
}

AIEngine::~AIEngine()
{
    unloadModel();
    if (m_vkdev) {
        delete m_vkdev;
    }
    ncnn::destroy_gpu_instance();
}

void AIEngine::initVulkan()
{
    // 初始化 Vulkan
    ncnn::create_gpu_instance();
    
    if (ncnn::get_gpu_count() > 0) {
        m_vkdev = new ncnn::VulkanDevice(ncnn::get_default_gpu_index());
        m_opt.use_vulkan_compute = true;
        m_opt.blob_vkallocator = m_vkdev->acquire_blob_allocator();
        m_opt.workspace_vkallocator = m_vkdev->acquire_staging_allocator();
        qDebug() << "Vulkan GPU acceleration enabled";
    } else {
        m_opt.use_vulkan_compute = false;
        qDebug() << "No GPU found, using CPU";
    }
    
    m_opt.num_threads = 4;
    m_opt.use_packing_layout = true;
}

void AIEngine::loadModelRegistry()
{
    QString modelsDir = QCoreApplication::applicationDirPath() + "/models";
    
    // 注册超分辨率模型
    m_modelRegistry["realesrgan_x4plus"] = {
        "realesrgan_x4plus",
        "Real-ESRGAN x4plus",
        "通用图像4倍超分辨率，真实图像效果最佳",
        ModelCategory::SuperResolution,
        modelsDir + "/super_resolution/realesrgan-x4plus.param",
        modelsDir + "/super_resolution/realesrgan-x4plus.bin",
        64 * 1024 * 1024,
        4,
        false,
        {}
    };
    
    m_modelRegistry["realesrgan_anime"] = {
        "realesrgan_anime",
        "Real-ESRGAN Anime",
        "动漫图像4倍超分辨率",
        ModelCategory::SuperResolutionAnime,
        modelsDir + "/super_resolution/realesrgan-x4plus-anime.param",
        modelsDir + "/super_resolution/realesrgan-x4plus-anime.bin",
        17 * 1024 * 1024,
        4,
        false,
        {}
    };
    
    m_modelRegistry["realcugan_x2"] = {
        "realcugan_x2",
        "Real-CUGAN 2x",
        "B站出品动漫2倍超分辨率",
        ModelCategory::SuperResolutionAnime,
        modelsDir + "/super_resolution/realcugan-x2.param",
        modelsDir + "/super_resolution/realcugan-x2.bin",
        2 * 1024 * 1024,
        2,
        false,
        {{"denoise", 0}}
    };
    
    m_modelRegistry["scunet"] = {
        "scunet",
        "SCUNet",
        "盲图像去噪，效果优秀",
        ModelCategory::Denoising,
        modelsDir + "/denoising/scunet.param",
        modelsDir + "/denoising/scunet.bin",
        9 * 1024 * 1024,
        1,
        false,
        {}
    };
    
    m_modelRegistry["zerodce_pp"] = {
        "zerodce_pp",
        "Zero-DCE++",
        "低光照增强，超轻量仅10KB",
        ModelCategory::LowLightEnhance,
        modelsDir + "/low_light/zerodce_pp.param",
        modelsDir + "/low_light/zerodce_pp.bin",
        10 * 1024,
        1,
        false,
        {}
    };
    
    m_modelRegistry["rife_v46"] = {
        "rife_v46",
        "RIFE v4.6",
        "实时视频插帧",
        ModelCategory::FrameInterpolation,
        modelsDir + "/frame_interp/rife-v4.6.param",
        modelsDir + "/frame_interp/rife-v4.6.bin",
        15 * 1024 * 1024,
        1,
        false,
        {}
    };
    
    // ... 注册更多模型
}

bool AIEngine::loadModel(const QString &modelId)
{
    if (!m_modelRegistry.contains(modelId)) {
        emit processError(QString("Model not found: %1").arg(modelId));
        return false;
    }
    
    if (m_currentModelId == modelId && m_currentModel.isLoaded) {
        return true; // 模型已加载
    }
    
    unloadModel();
    
    ModelInfo &info = m_modelRegistry[modelId];
    
    // 检查文件是否存在
    if (!QFile::exists(info.paramPath) || !QFile::exists(info.binPath)) {
        emit processError(QString("Model files not found: %1").arg(modelId));
        return false;
    }
    
    m_net.opt = m_opt;
    
    if (m_net.load_param(info.paramPath.toStdString().c_str()) != 0) {
        emit processError(QString("Failed to load param: %1").arg(info.paramPath));
        return false;
    }
    
    if (m_net.load_model(info.binPath.toStdString().c_str()) != 0) {
        emit processError(QString("Failed to load model: %1").arg(info.binPath));
        return false;
    }
    
    info.isLoaded = true;
    m_currentModelId = modelId;
    m_currentModel = info;
    
    emit modelLoaded(modelId);
    return true;
}

void AIEngine::unloadModel()
{
    if (!m_currentModelId.isEmpty()) {
        m_net.clear();
        m_modelRegistry[m_currentModelId].isLoaded = false;
        m_currentModelId.clear();
        emit modelUnloaded();
    }
}

QImage AIEngine::process(const QImage &input)
{
    if (m_currentModelId.isEmpty()) {
        emit processError("No model loaded");
        return QImage();
    }
    
    m_isProcessing = true;
    m_progress = 0.0;
    emit processingChanged(true);
    
    // 转换输入图像
    ncnn::Mat in = qimageToMat(input);
    m_progress = 0.2;
    emit progressChanged(m_progress);
    
    // 执行推理
    ncnn::Extractor ex = m_net.create_extractor();
    ex.set_vulkan_compute(m_opt.use_vulkan_compute);
    
    ex.input("input", in);
    m_progress = 0.4;
    emit progressChanged(m_progress);
    
    ncnn::Mat out;
    ex.extract("output", out);
    m_progress = 0.8;
    emit progressChanged(m_progress);
    
    // 转换输出图像
    QImage result = matToQimage(out);
    m_progress = 1.0;
    emit progressChanged(m_progress);
    
    m_isProcessing = false;
    emit processingChanged(false);
    emit processCompleted(result);
    
    return result;
}

ncnn::Mat AIEngine::qimageToMat(const QImage &image)
{
    QImage img = image.convertToFormat(QImage::Format_RGB888);
    int w = img.width();
    int h = img.height();
    
    ncnn::Mat in = ncnn::Mat::from_pixels(
        img.constBits(), 
        ncnn::Mat::PIXEL_RGB, 
        w, h
    );
    
    // 归一化到 [0, 1]
    const float norm[3] = {1/255.f, 1/255.f, 1/255.f};
    const float mean[3] = {0.f, 0.f, 0.f};
    in.substract_mean_normalize(mean, norm);
    
    return in;
}

QImage AIEngine::matToQimage(const ncnn::Mat &mat)
{
    int w = mat.w;
    int h = mat.h;
    
    // 反归一化
    ncnn::Mat out = mat.clone();
    const float denorm[3] = {255.f, 255.f, 255.f};
    const float mean[3] = {0.f, 0.f, 0.f};
    out.substract_mean_normalize(mean, denorm);
    
    QImage result(w, h, QImage::Format_RGB888);
    out.to_pixels(result.bits(), ncnn::Mat::PIXEL_RGB);
    
    return result;
}

QVariantList AIEngine::getAvailableModels(ModelCategory category)
{
    QVariantList list;
    for (const auto &info : m_modelRegistry) {
        if (info.category == category) {
            QVariantMap map;
            map["id"] = info.id;
            map["name"] = info.name;
            map["description"] = info.description;
            map["size"] = info.sizeBytes;
            map["scaleFactor"] = info.scaleFactor;
            map["isLoaded"] = info.isLoaded;
            list.append(map);
        }
    }
    return list;
}

} // namespace EnhanceVision
```

---

## 五、模型目录结构

```
resources/models/
├── super_resolution/
│   ├── realesrgan-x4plus.param
│   ├── realesrgan-x4plus.bin
│   ├── realesrgan-x4plus-anime.param
│   ├── realesrgan-x4plus-anime.bin
│   ├── realesr-animevideov3.param
│   ├── realesr-animevideov3.bin
│   ├── realcugan-x2.param
│   ├── realcugan-x2.bin
│   ├── realcugan-x3.param
│   ├── realcugan-x3.bin
│   ├── realcugan-x4.param
│   ├── realcugan-x4.bin
│   ├── waifu2x-cunet.param
│   ├── waifu2x-cunet.bin
│   ├── srmd-x2.param
│   ├── srmd-x2.bin
│   └── ...
├── denoising/
│   ├── scunet.param
│   ├── scunet.bin
│   ├── nafnet.param
│   ├── nafnet.bin
│   ├── dncnn.param
│   ├── dncnn.bin
│   └── ...
├── deblurring/
│   ├── mimo-unet.param
│   ├── mimo-unet.bin
│   ├── nafnet-deblur.param
│   ├── nafnet-deblur.bin
│   └── ...
├── dehazing/
│   ├── aod-net.param
│   ├── aod-net.bin
│   ├── ffa-net.param
│   ├── ffa-net.bin
│   └── ...
├── colorization/
│   ├── colorization-zhang.param
│   ├── colorization-zhang.bin
│   └── ...
├── inpainting/
│   ├── lama.param
│   ├── lama.bin
│   └── ...
├── low_light/
│   ├── zerodce.param
│   ├── zerodce.bin
│   ├── zerodce_pp.param
│   ├── zerodce_pp.bin
│   └── ...
└── frame_interp/
    ├── rife-v4.6.param
    ├── rife-v4.6.bin
    └── ...
```

---

## 六、技术方案可行性评估

### 6.1 可行性分析

| 评估维度 | 评估结果 | 说明 |
|----------|----------|------|
| **技术成熟度** | ✅ 高 | NCNN 是成熟的推理框架，社区活跃 |
| **模型覆盖度** | ✅ 高 | 66个模型涵盖所有主要画质增强场景 |
| **跨平台能力** | ✅ 优秀 | Windows/Linux/macOS/Android/iOS |
| **GPU 加速** | ✅ 支持 | Vulkan 跨平台 GPU 加速 |
| **部署复杂度** | ✅ 低 | 单一引擎，统一格式，简化维护 |
| **离线能力** | ✅ 完全 | 无网络依赖，纯本地运行 |
| **性能预期** | ✅ 良好 | Vulkan 加速下性能接近原生 |

### 6.2 潜在风险与应对

| 风险 | 影响 | 应对措施 |
|------|------|----------|
| **模型转换失败** | 中 | 使用 PNNX 替代 onnx2ncnn；简化模型结构 |
| **某些算子不支持** | 中 | 联系 NCNN 社区；实现自定义算子 |
| **大模型内存不足** | 中 | 实现分块处理；提供轻量版本 |
| **Vulkan 兼容性** | 低 | 自动回退到 CPU；提供 CPU 优化路径 |
| **模型效果差异** | 低 | 转换后充分测试；保留参考实现 |

### 6.3 性能预估

| 模型类型 | 输入尺寸 | GPU (RTX 3060) | CPU (i7-10700) |
|----------|----------|----------------|----------------|
| Real-ESRGAN 4x | 512x512 | ~200ms | ~2s |
| Real-CUGAN 2x | 1080p | ~300ms | ~3s |
| SCUNet | 1080p | ~150ms | ~1.5s |
| Zero-DCE++ | 1080p | ~20ms | ~200ms |
| RIFE | 1080p→2帧 | ~30ms | ~300ms |

---

## 七、实施路线图

### 第一阶段：核心功能（4周）

| 周次 | 任务 | 交付物 |
|------|------|--------|
| Week 1 | 集成 NCNN 框架 | AIEngine 基础架构 |
| Week 2 | 实现超分辨率模型 | Real-ESRGAN、Real-CUGAN |
| Week 3 | 实现去噪模型 | SCUNet、NAFNet |
| Week 4 | UI 集成与测试 | 可用的 MVP 版本 |

### 第二阶段：扩展功能（4周）

| 周次 | 任务 | 交付物 |
|------|------|--------|
| Week 5 | 去模糊、去雾模型 | MIMO-UNet、FFA-Net |
| Week 6 | 低光增强模型 | Zero-DCE++、SCI |
| Week 7 | 图像修复模型 | LaMa、AOT-GAN |
| Week 8 | 视频插帧模型 | RIFE v4.6 |

### 第三阶段：优化完善（4周）

| 周次 | 任务 | 交付物 |
|------|------|--------|
| Week 9 | 性能优化 | 分块处理、内存优化 |
| Week 10 | 批量处理 | 多图/视频批处理 |
| Week 11 | 模型管理 UI | 模型下载、切换界面 |
| Week 12 | 测试与文档 | 完整测试、用户文档 |

---

## 八、注意事项

### 8.1 开发注意事项

1. **内存管理**
   - 大图处理需要分块（tile-based processing）
   - 及时释放不用的 ncnn::Mat
   - 使用 NCNN 的内存池管理

2. **线程安全**
   - ncnn::Net 不是线程安全的
   - 每个线程使用独立的 Extractor
   - 推理操作放在工作线程

3. **Vulkan 初始化**
   - 在应用启动时初始化一次
   - 检测 GPU 可用性并优雅降级
   - 正确释放 Vulkan 资源

4. **模型输入输出**
   - 注意不同模型的输入输出节点名称
   - 处理不同的归一化方式
   - 支持动态输入尺寸

### 8.2 部署注意事项

1. **依赖打包**
   - 包含 NCNN 动态库（~15MB）
   - Vulkan 运行时（系统自带或打包）
   - 模型文件单独管理

2. **模型分发**
   - 核心模型随应用打包（~100MB）
   - 扩展模型按需下载
   - 支持用户自定义模型

3. **版本兼容**
   - NCNN 版本与模型格式匹配
   - 记录模型转换工具版本
   - 提供模型版本检查

### 8.3 性能优化建议

1. **GPU 优先**
   - 默认启用 Vulkan 加速
   - 提供 GPU/CPU 切换选项
   - 监控 GPU 内存使用

2. **分块处理**
   - 大图自动分块处理
   - 块边界处理（overlap）
   - 进度显示

3. **缓存机制**
   - 模型热加载保持在内存
   - 中间结果缓存
   - 预加载常用模型

---

## 九、总结

### 最终方案确认

| 项目 | 决策 |
|------|------|
| **推理引擎** | NCNN（唯一） |
| **模型格式** | .param / .bin |
| **GPU 加速** | Vulkan |
| **模型数量** | 66 个 |
| **核心模型** | 25 个 |
| **预估大小** | ~1.2GB（全部） / ~200MB（核心） |
| **部署方式** | 纯本地离线 |

### 核心优势

1. **单一引擎**：维护简单，无复杂依赖
2. **跨平台**：Windows/Linux/macOS 完美支持
3. **高性能**：Vulkan GPU 加速
4. **轻量级**：运行时仅 ~15MB
5. **离线运行**：无网络依赖
6. **模型丰富**：覆盖所有画质增强场景

---

*文档创建时间：2026-03-21*  
*版本：v1.0*  
*适用项目：EnhanceVision*  
*技术栈：Qt 6.10.2 + QML + NCNN + Vulkan*
