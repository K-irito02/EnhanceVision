# 免费画质增强模型与推理引擎技术评估

> **文档说明**：本文档整理了主流免费开源画质增强模型及其在各推理引擎上的支持情况，经过验证和完善。
>
> **适用项目**：EnhanceVision
>
> **最后更新**：2026-03-21

---

## 一、支持标记说明

| 标记 | 含义 | 说明 |
|------|------|------|
| ✅ | **原生支持** | 官方提供转换模型或转换流程成熟稳定 |
| ⚠️ | **需要转换** | 可通过 PNNX/onnx2ncnn 等工具转换，但可能需要调整 |
| 🔧 | **需要适配** | 需要自定义算子或较多修改才能运行 |
| ❌ | **不支持** | 架构不兼容或转换后无法正常工作 |
| 🔶 | **非神经网络** | 该模型是基于 Shader/算法实现，不适用于神经网络推理引擎 |

---

## 二、超分辨率模型（Super Resolution）

### 2.1 通用超分辨率模型

| 模型名称 | 放大倍数 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|----------|----------|------|--------------|----------|------|----------|
| **Real-ESRGAN x4plus** | 4x | 64MB | ✅ | ✅ | ✅ | 真实图像增强效果最佳 | [GitHub](https://github.com/xinntao/Real-ESRGAN) |
| **Real-ESRGAN x2plus** | 2x | 64MB | ✅ | ✅ | ✅ | 2倍放大版本 | 同上 |
| **RealESRNet x4plus** | 4x | 64MB | ✅ | ✅ | ✅ | PSNR 导向版本 | 同上 |
| **ESRGAN** | 4x | 64MB | ✅ | ✅ | ✅ | 经典 GAN 超分模型 | [GitHub](https://github.com/xinntao/ESRGAN) |
| **BSRGAN** | 4x | 64MB | ✅ | ✅ | ✅ | 盲超分辨率，真实退化 | [GitHub](https://github.com/cszn/BSRGAN) |
| **SwinIR** | 2x/4x | 12-47MB | ⚠️ | ✅ | ✅ | Transformer 架构，效果领先 | [GitHub](https://github.com/JingyunLiang/SwinIR) |
| **SwinIR-L** | 4x | 137MB | ⚠️ | ✅ | ✅ | SwinIR 大模型版本 | 同上 |
| **HAT** | 4x | 40-92MB | ⚠️ | ✅ | ✅ | 混合注意力 Transformer | [GitHub](https://github.com/XPixelGroup/HAT) |
| **HAT-L** | 4x | 200MB+ | ⚠️ | ✅ | ⚠️ | HAT 大模型，显存要求高 | 同上 |
| **DAT** | 2x/4x | 40-150MB | ⚠️ | ✅ | ✅ | 双聚合 Transformer | [GitHub](https://github.com/zhengchen1999/DAT) |
| **OmniSR** | 4x | 15MB | ⚠️ | ✅ | ✅ | 轻量级高效超分 | [GitHub](https://github.com/Francis0625/Omni-SR) |
| **DRCT** | 4x | 50MB | ⚠️ | ✅ | ✅ | 密集残差连接 Transformer | [GitHub](https://github.com/ming053l/DRCT) |
| **SRFormer** | 4x | 25-150MB | ⚠️ | ✅ | ✅ | 置换自注意力 | [GitHub](https://github.com/HVision-NKU/SRFormer) |
| **EDSR** | 2x/3x/4x | 40-170MB | ✅ | ✅ | ✅ | 增强深度残差网络 | [GitHub](https://github.com/sanghyun-son/EDSR-PyTorch) |
| **RCAN** | 2x/4x | 60MB | ✅ | ✅ | ✅ | 残差通道注意力 | [GitHub](https://github.com/yulunzhang/RCAN) |
| **SRResNet** | 4x | 6MB | ✅ | ✅ | ✅ | 轻量级残差网络 | [GitHub](https://github.com/twtygqyy/pytorch-SRResNet) |
| **SRCNN** | 2x/3x/4x | 0.2MB | ✅ | ✅ | ✅ | 经典开山之作，极轻量 | [GitHub](https://github.com/yjn870/SRCNN-pytorch) |
| **VDSR** | 2x/3x/4x | 2.7MB | ✅ | ✅ | ✅ | 深度超分辨率 | [GitHub](https://github.com/twtygqyy/pytorch-vdsr) |
| **CARN** | 2x/4x | 6MB | ✅ | ✅ | ✅ | 级联残差网络，轻量 | [GitHub](https://github.com/nmhkahn/CARN-pytorch) |
| **IMDN** | 4x | 0.7MB | ✅ | ✅ | ✅ | 信息多蒸馏网络，极轻量 | [GitHub](https://github.com/Zheng222/IMDN) |
| **PAN** | 4x | 0.3MB | ✅ | ✅ | ✅ | 像素注意力网络，超轻量 | [GitHub](https://github.com/zhaohengyuan1/PAN) |
| **ESRT** | 4x | 0.8MB | ⚠️ | ✅ | ✅ | 高效 Transformer | [GitHub](https://github.com/luissen/ESRT) |
| **FeMaSR** | 4x | 80MB | ⚠️ | ✅ | ✅ | 特征匹配超分 | [GitHub](https://github.com/chaofengc/FeMaSR) |
| **RealSR** | 4x | 64MB | ✅ | ✅ | ✅ | 真实世界超分 | [GitHub](https://github.com/jixiaozhong/RealSR) |

### 2.2 动漫专用超分辨率模型

| 模型名称 | 放大倍数 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|----------|----------|------|--------------|----------|------|----------|
| **Real-ESRGAN Anime** | 4x | 17MB | ✅ | ✅ | ✅ | 动漫专用，速度快 | [GitHub](https://github.com/xinntao/Real-ESRGAN) |
| **Real-ESRGAN AnimeVideo v3** | 4x | 3MB | ✅ | ✅ | ✅ | 动漫视频专用，超轻量 | 同上 |
| **Waifu2x CUNet** | 2x | 3MB | ✅ | ✅ | ✅ | 经典动漫超分 | [GitHub](https://github.com/nagadomi/waifu2x) |
| **Waifu2x UpConv** | 2x | 1MB | ✅ | ✅ | ✅ | Waifu2x 轻量版 | 同上 |
| **Real-CUGAN** | 2x/3x/4x | 1-7MB | ✅ | ✅ | ✅ | B站出品，效果优秀 | [GitHub](https://github.com/bilibili/ailab/tree/main/Real-CUGAN) |
| **Real-CUGAN Pro** | 2x/3x | 2-10MB | ✅ | ✅ | ✅ | 专业版，效果更好 | 同上 |
| **APISR** | 2x/4x | 30-60MB | ⚠️ | ✅ | ✅ | 动漫生产级超分 | [GitHub](https://github.com/Kiteretsu77/APISR) |
| **Anime4K** | 2x/4x | <0.1MB | 🔶 | 🔶 | 🔶 | **Shader 算法**，实时处理 | [GitHub](https://github.com/bloc97/Anime4K) |
| **ACNet** | 2x | 1MB | ✅ | ✅ | ✅ | 动漫专用 CNN | [GitHub](https://github.com/TianZerL/Anime4KCPP) |
| **SRMD** | 2x/3x/4x | 6MB | ✅ | ✅ | ✅ | 多退化超分 | [GitHub](https://github.com/cszn/SRMD) |

### 2.3 视频超分辨率模型

| 模型名称 | 放大倍数 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|----------|----------|------|--------------|----------|------|----------|
| **BasicVSR** | 4x | 25MB | ⚠️ | ✅ | ✅ | 视频超分基础模型 | [GitHub](https://github.com/ckkelvinchan/BasicVSR-IconVSR) |
| **BasicVSR++** | 4x | 30MB | ⚠️ | ✅ | ✅ | 改进版，效果更好 | [GitHub](https://github.com/ckkelvinchan/BasicVSR_PlusPlus) |
| **RealBasicVSR** | 4x | 50MB | ⚠️ | ✅ | ✅ | 真实视频超分 | [GitHub](https://github.com/ckkelvinchan/RealBasicVSR) |
| **EDVR** | 4x | 23MB | ⚠️ | ✅ | ✅ | 视频增强去模糊恢复 | [GitHub](https://github.com/xinntao/EDVR) |
| **IconVSR** | 4x | 30MB | ⚠️ | ✅ | ✅ | 信息耦合视频超分 | [GitHub](https://github.com/ckkelvinchan/BasicVSR-IconVSR) |
| **RVRT** | 4x | 50MB | 🔧 | ✅ | ⚠️ | 循环视频恢复 Transformer | [GitHub](https://github.com/JingyunLiang/RVRT) |
| **VRT** | 4x | 70MB | 🔧 | ✅ | ⚠️ | 视频恢复 Transformer | [GitHub](https://github.com/JingyunLiang/VRT) |

---

## 三、图像去噪模型（Denoising）

| 模型名称 | 用途 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|------|----------|------|--------------|----------|------|----------|
| **DnCNN** | 通用去噪 | 0.7MB | ✅ | ✅ | ✅ | 经典去噪网络 | [GitHub](https://github.com/cszn/DnCNN) |
| **FFDNet** | 灵活去噪 | 0.9MB | ✅ | ✅ | ✅ | 可调噪声级别 | [GitHub](https://github.com/cszn/FFDNet) |
| **SCUNet** | 盲去噪 | 9MB | ✅ | ✅ | ✅ | Swin-Conv 混合架构 | [GitHub](https://github.com/cszn/SCUNet) |
| **SCUNet-GAN** | 盲去噪 | 9MB | ✅ | ✅ | ✅ | GAN 版本，视觉效果更好 | 同上 |
| **NAFNet** | 多任务 | 8-67MB | ✅ | ✅ | ✅ | 非线性激活自由网络 | [GitHub](https://github.com/megvii-research/NAFNet) |
| **Restormer** | 多任务 | 26-100MB | ⚠️ | ✅ | ✅ | 高效 Transformer 恢复 | [GitHub](https://github.com/swz30/Restormer) |
| **CBDNet** | 真实噪声 | 16MB | ✅ | ✅ | ✅ | 盲卷积去噪 | [GitHub](https://github.com/GuoShi28/CBDNet) |
| **RIDNet** | 真实噪声 | 6MB | ✅ | ✅ | ✅ | 残差图像去噪 | [GitHub](https://github.com/saeed-anwar/RIDNet) |
| **MIRNet** | 真实噪声 | 31MB | ✅ | ✅ | ✅ | 多尺度残差块 | [GitHub](https://github.com/swz30/MIRNet) |
| **MIRNet-v2** | 真实噪声 | 6MB | ✅ | ✅ | ✅ | 轻量级改进版 | [GitHub](https://github.com/swz30/MIRNetv2) |
| **MPRNet** | 多任务 | 20MB | ✅ | ✅ | ✅ | 多阶段渐进恢复 | [GitHub](https://github.com/swz30/MPRNet) |
| **VDN** | 变分去噪 | 8MB | ✅ | ✅ | ✅ | 变分推断方法 | [GitHub](https://github.com/zsyOAOA/VDN) |
| **SwinIR-Denoise** | 去噪 | 12MB | ⚠️ | ✅ | ✅ | SwinIR 去噪版本 | [GitHub](https://github.com/JingyunLiang/SwinIR) |

---

## 四、人脸修复模型（Face Restoration）

| 模型名称 | 用途 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|------|----------|------|--------------|----------|------|----------|
| **GFPGAN v1.2** | 人脸修复 | 330MB | ⚠️ | ✅ | ✅ | 生成式人脸先验 | [GitHub](https://github.com/TencentARC/GFPGAN) |
| **GFPGAN v1.3** | 人脸修复 | 330MB | ⚠️ | ✅ | ✅ | 改进眼睛和牙齿 | 同上 |
| **GFPGAN v1.4** | 人脸修复 | 330MB | ⚠️ | ✅ | ✅ | 最新版，效果最好 | 同上 |
| **CodeFormer** | 人脸修复 | 170MB | ⚠️ | ✅ | ✅ | 代码本查找，极端退化效果好 | [GitHub](https://github.com/sczhou/CodeFormer) |
| **RestoreFormer** | 人脸修复 | 280MB | ⚠️ | ✅ | ⚠️ | 高质量盲人脸修复 | [GitHub](https://github.com/wzhouxiff/RestoreFormer) |
| **RestoreFormer++** | 人脸修复 | 290MB | ⚠️ | ✅ | ⚠️ | 改进版 | 同上 |
| **VQFR** | 人脸修复 | 150MB | ⚠️ | ✅ | ⚠️ | 向量量化人脸修复 | [GitHub](https://github.com/TencentARC/VQFR) |
| **GPEN** | 人脸修复 | 250MB | ⚠️ | ✅ | ⚠️ | GAN 先验嵌入网络 | [GitHub](https://github.com/yangxy/GPEN) |
| **DFDNet** | 人脸修复 | 150MB | ⚠️ | ✅ | ⚠️ | 字典引导修复 | [GitHub](https://github.com/csxmli2016/DFDNet) |
| **PSFRGAN** | 人脸修复 | 120MB | ⚠️ | ✅ | ⚠️ | 渐进语义引导 | [GitHub](https://github.com/chaofengc/PSFRGAN) |
| **DiffBIR** | 通用修复 | 2GB+ | 🔧 | ⚠️ | ❌ | **扩散模型**，效果惊艳但部署复杂 | [GitHub](https://github.com/XPixelGroup/DiffBIR) |
| **SUPIR** | 通用修复 | 5GB+ | ❌ | ❌ | ❌ | **扩散模型**，当前最佳效果但无法轻量部署 | [GitHub](https://github.com/Fanghua-Yu/SUPIR) |

> **注意**：DiffBIR 和 SUPIR 基于 Stable Diffusion，模型体积巨大，实际部署需要高性能 GPU 和复杂的环境配置，不适合轻量级桌面应用。

---

## 五、图像去模糊模型（Deblurring）

| 模型名称 | 用途 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|------|----------|------|--------------|----------|------|----------|
| **DeblurGAN** | 运动去模糊 | 11MB | ✅ | ✅ | ✅ | GAN 去模糊 | [GitHub](https://github.com/KupynOrest/DeblurGAN) |
| **DeblurGAN-v2** | 运动去模糊 | 15-60MB | ✅ | ✅ | ✅ | 改进版，多种backbone | [GitHub](https://github.com/VITA-Group/DeblurGANv2) |
| **MIMO-UNet** | 运动去模糊 | 6MB | ✅ | ✅ | ✅ | 多输入多输出 | [GitHub](https://github.com/chosj95/MIMO-UNet) |
| **MIMO-UNet+** | 运动去模糊 | 16MB | ✅ | ✅ | ✅ | 增强版 | 同上 |
| **NAFNet-Deblur** | 运动去模糊 | 8-67MB | ✅ | ✅ | ✅ | 通用高效模型 | [GitHub](https://github.com/megvii-research/NAFNet) |
| **HINet** | 多任务 | 89MB | ✅ | ✅ | ✅ | 半实例归一化 | [GitHub](https://github.com/megvii-model/HINet) |
| **Stripformer** | 运动去模糊 | 20MB | ⚠️ | ✅ | ✅ | 条带 Transformer | [GitHub](https://github.com/pp00704831/Stripformer) |
| **FFTformer** | 运动去模糊 | 60MB | ⚠️ | ✅ | ✅ | 频域 Transformer | [GitHub](https://github.com/kkkls/FFTformer) |
| **MAXIM** | 多任务 | 58MB | ⚠️ | ✅ | ✅ | 多轴 MLP | [GitHub](https://github.com/google-research/maxim) |

---

## 六、图像去雾模型（Dehazing）

| 模型名称 | 用途 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|------|----------|------|--------------|----------|------|----------|
| **AOD-Net** | 去雾 | 0.1MB | ✅ | ✅ | ✅ | 端到端轻量级 | [GitHub](https://github.com/Boyiliee/AOD-Net) |
| **DehazeNet** | 去雾 | 0.3MB | ✅ | ✅ | ✅ | 经典去雾网络 | [GitHub](https://github.com/caibolun/DehazeNet) |
| **FFA-Net** | 去雾 | 4MB | ✅ | ✅ | ✅ | 特征融合注意力 | [GitHub](https://github.com/zhilin007/FFA-Net) |
| **MSBDN-DFF** | 去雾 | 31MB | ✅ | ✅ | ✅ | 多尺度增强去雾 | [GitHub](https://github.com/zzr-idam/MSBDN-DFF) |
| **GridDehazeNet** | 去雾 | 1MB | ✅ | ✅ | ✅ | 网格去雾网络 | [GitHub](https://github.com/proteus1991/GridDehazeNet) |
| **AECR-Net** | 去雾 | 2.6MB | ✅ | ✅ | ✅ | 对比正则化 | [GitHub](https://github.com/GlassyWu/AECR-Net) |
| **Dehamer** | 去雾 | 130MB | ⚠️ | ✅ | ✅ | Transformer 去雾 | [GitHub](https://github.com/Li-Chongyi/Dehamer) |
| **DehazeFormer** | 去雾 | 25MB | ⚠️ | ✅ | ✅ | 高效 Transformer | [GitHub](https://github.com/IDKiro/DehazeFormer) |

---

## 七、图像上色模型（Colorization）

| 模型名称 | 用途 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|------|----------|------|--------------|----------|------|----------|
| **DDColor** | 黑白上色 | 100MB | ⚠️ | ✅ | ✅ | **ICCV 2023**，效果最佳 | [GitHub](https://github.com/piddnad/DDColor) |
| **DeOldify Stable** | 黑白上色 | 100MB | ⚠️ | ✅ | ⚠️ | 稳定版，色彩稳定 | [GitHub](https://github.com/jantic/DeOldify) |
| **DeOldify Artistic** | 黑白上色 | 100MB | ⚠️ | ✅ | ⚠️ | 艺术版，色彩鲜艳 | 同上 |
| **Colorization (Zhang)** | 黑白上色 | 129MB | ✅ | ✅ | ✅ | ECCV 2016 经典方法 | [GitHub](https://github.com/richzhang/colorization) |
| **ChromaGAN** | 黑白上色 | 70MB | ✅ | ✅ | ✅ | GAN 上色 | [GitHub](https://github.com/pvitoria/ChromaGAN) |
| **InstColorization** | 实例上色 | 200MB | ⚠️ | ✅ | ⚠️ | 实例感知上色 | [GitHub](https://github.com/ericsujw/InstColorization) |
| **BigColor** | 大图上色 | 150MB | ⚠️ | ✅ | ⚠️ | 支持大图 | [GitHub](https://github.com/DmitryUlyanov/deep-image-prior) |

---

## 八、图像修复模型（Inpainting）

| 模型名称 | 用途 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|------|----------|------|--------------|----------|------|----------|
| **LaMa** | 图像修复 | 51MB | ✅ | ✅ | ✅ | 大掩码修复，效果优秀 | [GitHub](https://github.com/advimman/lama) |
| **LaMa-Fourier** | 图像修复 | 51MB | ✅ | ✅ | ✅ | 傅里叶卷积版 | 同上 |
| **MAT** | 图像修复 | 250MB | ⚠️ | ✅ | ⚠️ | 掩码感知 Transformer | [GitHub](https://github.com/fenglinglwb/MAT) |
| **AOT-GAN** | 图像修复 | 12MB | ✅ | ✅ | ✅ | 聚合上下文 | [GitHub](https://github.com/researchmm/AOT-GAN-for-Inpainting) |
| **DeepFill v2** | 图像修复 | 20MB | ✅ | ✅ | ✅ | 门控卷积 | [GitHub](https://github.com/JiahuiYu/generative_inpainting) |
| **EdgeConnect** | 图像修复 | 15MB | ✅ | ✅ | ✅ | 边缘引导修复 | [GitHub](https://github.com/knazeri/edge-connect) |
| **PartialConv** | 图像修复 | 15MB | ✅ | ✅ | ✅ | 部分卷积 | [GitHub](https://github.com/NVIDIA/partialconv) |
| **CTSDG** | 图像修复 | 30MB | ⚠️ | ✅ | ✅ | 纹理结构双引导 | [GitHub](https://github.com/xiefan-guo/ctsdg) |

---

## 九、低光照增强模型（Low-Light Enhancement）

| 模型名称 | 用途 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|------|----------|------|--------------|----------|------|----------|
| **Zero-DCE** | 低光增强 | 0.3MB | ✅ | ✅ | ✅ | 零参考深度曲线估计 | [GitHub](https://github.com/Li-Chongyi/Zero-DCE) |
| **Zero-DCE++** | 低光增强 | 0.01MB | ✅ | ✅ | ✅ | **超轻量**，仅 10KB | 同上 |
| **EnlightenGAN** | 低光增强 | 33MB | ✅ | ✅ | ✅ | 无监督 GAN | [GitHub](https://github.com/VITA-Group/EnlightenGAN) |
| **RetinexNet** | 低光增强 | 2MB | ✅ | ✅ | ✅ | Retinex 分解 | [GitHub](https://github.com/weichen582/RetinexNet) |
| **KinD** | 低光增强 | 8MB | ✅ | ✅ | ✅ | Retinex 启发 | [GitHub](https://github.com/zhangyhuaee/KinD) |
| **KinD++** | 低光增强 | 8MB | ✅ | ✅ | ✅ | 改进版 | [GitHub](https://github.com/zhangyhuaee/KinD_plus) |
| **SCI** | 低光增强 | 0.01MB | ✅ | ✅ | ✅ | 自校准照明，超轻量 | [GitHub](https://github.com/vis-opt-group/SCI) |
| **LLFlow** | 低光增强 | 175MB | ⚠️ | ✅ | ⚠️ | 流模型，高质量 | [GitHub](https://github.com/wyf0912/LLFlow) |
| **SNR-Aware** | 低光增强 | 25MB | ⚠️ | ✅ | ✅ | 信噪比感知 | [GitHub](https://github.com/dvlab-research/SNR-Aware-Low-Light-Enhance) |
| **URetinex-Net** | 低光增强 | 1MB | ✅ | ✅ | ✅ | 展开 Retinex | [GitHub](https://github.com/AndersonYong/URetinex-Net) |
| **RUAS** | 低光增强 | 0.003MB | ✅ | ✅ | ✅ | 搜索得到的极轻量架构 | [GitHub](https://github.com/KarelZhang/RUAS) |

---

## 十、视频插帧模型（Frame Interpolation）

| 模型名称 | 用途 | 模型大小 | NCNN | ONNX Runtime | TensorRT | 特点 | 开源地址 |
|----------|------|----------|------|--------------|----------|------|----------|
| **RIFE** | 视频插帧 | 7-15MB | ✅ | ✅ | ✅ | 实时中间帧估计 | [GitHub](https://github.com/megvii-research/ECCV2022-RIFE) |
| **RIFE v4.6** | 视频插帧 | 15MB | ✅ | ✅ | ✅ | 最新稳定版 | 同上 |
| **IFRNet** | 视频插帧 | 20MB | ✅ | ✅ | ✅ | 高效中间帧合成 | [GitHub](https://github.com/ltkong218/IFRNet) |
| **CAIN** | 视频插帧 | 40MB | ✅ | ✅ | ✅ | 通道注意力插帧 | [GitHub](https://github.com/myungsub/CAIN) |
| **DAIN** | 视频插帧 | 200MB | ⚠️ | ✅ | ⚠️ | 深度感知插帧 | [GitHub](https://github.com/baowenbo/DAIN) |
| **FLAVR** | 视频插帧 | 100MB | ⚠️ | ✅ | ⚠️ | 3D 卷积插帧 | [GitHub](https://github.com/tarun005/FLAVR) |
| **AMT** | 视频插帧 | 30MB | ⚠️ | ✅ | ✅ | 所有时刻 Transformer | [GitHub](https://github.com/MCG-NKU/AMT) |

---

## 十一、各推理引擎综合对比

### 11.1 模型支持数量统计

| 模型类别 | 总数 | NCNN 原生 | NCNN 转换后 | ONNX Runtime | TensorRT |
|----------|------|-----------|-------------|--------------|----------|
| **超分辨率（通用）** | 24 | 15 | 22 | 24 | 23 |
| **超分辨率（动漫）** | 10 | 8 | 9 | 9 | 9 |
| **超分辨率（视频）** | 7 | 0 | 5 | 7 | 5 |
| **去噪** | 13 | 10 | 12 | 13 | 13 |
| **人脸修复** | 12 | 0 | 8 | 10 | 8 |
| **去模糊** | 9 | 6 | 8 | 9 | 9 |
| **去雾** | 8 | 6 | 8 | 8 | 8 |
| **上色** | 7 | 2 | 5 | 7 | 5 |
| **图像修复** | 8 | 5 | 7 | 8 | 7 |
| **低光增强** | 11 | 9 | 10 | 11 | 10 |
| **视频插帧** | 7 | 3 | 5 | 7 | 5 |
| **合计** | **116** | **64** | **99** | **113** | **102** |

### 11.2 推理引擎特性对比

| 特性 | NCNN | ONNX Runtime | TensorRT | MNN | OpenVINO |
|------|------|--------------|----------|-----|----------|
| **跨平台** | ✅ 全平台 | ✅ 全平台 | ❌ 仅 NVIDIA | ✅ 全平台 | ⚠️ 主要 x86 |
| **GPU 加速** | Vulkan | CUDA/DirectML | CUDA | Vulkan/Metal | OpenCL |
| **模型格式** | .param/.bin | .onnx | .engine | .mnn | .xml/.bin |
| **部署大小** | ~15MB | ~50MB | ~300MB | ~10MB | ~100MB |
| **移动端支持** | ✅ 优秀 | ⚠️ 一般 | ❌ 不支持 | ✅ 优秀 | ❌ 不支持 |
| **Transformer** | ⚠️ PNNX | ✅ 原生 | ✅ 原生 | ⚠️ 部分 | ✅ 原生 |
| **动态形状** | ⚠️ 有限 | ✅ 支持 | ⚠️ 有限 | ⚠️ 有限 | ✅ 支持 |
| **量化支持** | INT8/FP16 | INT8/FP16 | INT8/FP16 | INT8/FP16 | INT8 |

### 11.3 关键发现与建议

#### ✅ 重要发现

1. **ONNX 是最通用的中间格式**
   - 几乎所有 PyTorch 模型都可以导出为 ONNX
   - ONNX Runtime 对所有模型类型支持最完整
   
2. **NCNN 在轻量模型上表现最佳**
   - CNN 类模型（ESRGAN、Waifu2x 等）支持优秀
   - Transformer 模型需要通过 PNNX 转换，可能有兼容问题
   
3. **人脸修复和生成式模型部署复杂**
   - GFPGAN、CodeFormer 等 GAN 模型体积大，需要优化
   - DiffBIR、SUPIR 等扩散模型目前不适合轻量部署
   
4. **视频模型需要特殊处理**
   - 多帧输入需要状态管理
   - 显存占用较高

#### 🎯 EnhanceVision 项目推荐

**第一优先级（核心功能）**：
| 模型 | 类型 | 推理引擎 | 理由 |
|------|------|----------|------|
| Real-ESRGAN x4plus | 超分辨率 | NCNN | 效果好、支持完善 |
| Real-ESRGAN Anime | 超分辨率 | NCNN | 动漫专用、官方支持 |
| Real-CUGAN | 超分辨率 | NCNN | B站出品、效果优秀 |
| SCUNet | 去噪 | NCNN | 效果好、体积小 |
| GFPGAN v1.4 | 人脸修复 | ONNX | 成熟稳定 |

**第二优先级（扩展功能）**：
| 模型 | 类型 | 推理引擎 | 理由 |
|------|------|----------|------|
| CodeFormer | 人脸修复 | ONNX | 极端退化效果最好 |
| NAFNet | 去噪/去模糊 | NCNN | 多功能、高效 |
| Zero-DCE++ | 低光增强 | NCNN | 超轻量 |
| LaMa | 图像修复 | NCNN | 去水印效果好 |
| DDColor | 上色 | ONNX | 最新最佳效果 |

**第三优先级（可选功能）**：
| 模型 | 类型 | 推理引擎 | 理由 |
|------|------|----------|------|
| SwinIR | 超分辨率 | ONNX | 质量高但速度慢 |
| RIFE | 视频插帧 | NCNN | 实时插帧 |
| DehazeFormer | 去雾 | ONNX | 场景特定 |

---

## 十二、模型转换工具链

### 12.1 PyTorch → ONNX → NCNN

```bash
# 步骤 1: PyTorch 导出 ONNX
python export_onnx.py --model realesrgan --output model.onnx

# 步骤 2: 简化 ONNX（推荐）
python -m onnxsim model.onnx model_sim.onnx

# 步骤 3: ONNX 转 NCNN
# 方式 A: 传统 onnx2ncnn（适用于简单模型）
./onnx2ncnn model_sim.onnx model.param model.bin

# 方式 B: PNNX（推荐，支持更多算子）
./pnnx model_sim.onnx inputshape=[1,3,720,1280]
```

### 12.2 模型获取资源

| 资源 | 地址 | 说明 |
|------|------|------|
| **OpenModelDB** | https://openmodeldb.info/ | 预训练超分模型集合 |
| **BasicSR** | https://github.com/XPixelGroup/BasicSR | 图像恢复工具箱 |
| **ONNX Model Zoo** | https://github.com/onnx/models | 官方 ONNX 模型 |
| **Hugging Face** | https://huggingface.co/models | 大量预训练模型 |
| **Real-ESRGAN Releases** | https://github.com/xinntao/Real-ESRGAN/releases | 官方预转换模型 |
| **NCNN Model Zoo** | https://github.com/nihui/ncnn-assets | NCNN 格式模型 |

---

## 十三、总结

### 推荐技术方案

对于 **EnhanceVision** 项目，推荐采用 **NCNN + ONNX Runtime 混合架构**：

```
┌─────────────────────────────────────────────────────────────┐
│                    推理引擎选择策略                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                  模型类型判断                        │   │
│  └─────────────────────────────────────────────────────┘   │
│                            │                                │
│         ┌─────────────────┼─────────────────┐              │
│         ↓                  ↓                 ↓              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │ CNN 类模型  │  │ Transformer │  │  GAN/扩散   │        │
│  │ Real-ESRGAN │  │   SwinIR    │  │   GFPGAN    │        │
│  │  Waifu2x   │  │    HAT      │  │ CodeFormer  │        │
│  │  SCUNet    │  │    DAT      │  │   DiffBIR   │        │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘        │
│         ↓                 ↓                 ↓              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │    NCNN     │  │ ONNX Runtime│  │ ONNX Runtime│        │
│  │  (首选)     │  │  (首选)      │  │  (可选云端) │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 实施建议

1. **第一阶段**：集成 NCNN，支持 Real-ESRGAN、Real-CUGAN、SCUNet
2. **第二阶段**：添加 ONNX Runtime，支持 GFPGAN、CodeFormer
3. **第三阶段**：扩展更多模型类型（去模糊、低光增强等）
4. **第四阶段**：建立模型仓库，支持用户自定义模型

---

*文档创建时间：2026-03-21*  
*最后更新：2026-03-21*  
*适用项目：EnhanceVision*  
*技术栈：Qt 6.10.2 + QML + NCNN + ONNX Runtime + Vulkan*
