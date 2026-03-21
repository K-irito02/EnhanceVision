# Tasks

- [x] Task 1: 完善 models.json 模型参数配置
  - [x] SubTask 1.1: 为超分辨率模型添加 supportedParams（Real-ESRGAN、Waifu2x、SRMD）
  - [x] SubTask 1.2: 为去噪模型添加 supportedParams（SCUNet、NAFNet）
  - [x] SubTask 1.3: 为去模糊模型添加 supportedParams（MPRNet）
  - [x] SubTask 1.4: 为其他模型添加 supportedParams（FFA-Net、DeOldify、Zero-DCE）
  - [x] SubTask 1.5: 添加 RIFE 视频插帧模型配置

- [x] Task 2: 增强 AIParamsPanel 参数类型支持
  - [x] SubTask 2.1: 添加 bool 类型参数的 Switch 控件支持
  - [x] SubTask 2.2: 添加 string 类型参数的 TextField 控件支持
  - [x] SubTask 2.3: 实现参数标签国际化（label/label_en）
  - [x] SubTask 2.4: 添加参数描述提示功能

- [x] Task 3: 优化 AIEngine 参数处理
  - [x] SubTask 3.1: 实现参数默认值合并逻辑
  - [x] SubTask 3.2: 添加参数范围验证
  - [x] SubTask 3.3: 优化参数传递到推理流程

- [x] Task 4: 更新 ModelRegistry 支持新参数类型
  - [x] SubTask 4.1: 确保参数元数据正确解析
  - [x] SubTask 4.2: 添加参数验证辅助方法

- [x] Task 5: 使用 qt-build-and-fix 技能运行项目验证
  - [x] SubTask 5.1: 构建项目
  - [x] SubTask 5.2: 运行并测试 AI 模型选择和参数配置功能

# Task Dependencies

- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 1]
- [Task 5] depends on [Task 1, Task 2, Task 3, Task 4]
