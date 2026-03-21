# EnhanceVision AI 推理功能完整实现 - Task List

## [ ] Task 1: AIEngine 核心框架实现
- **Priority**: P0
- **Depends On**: None
- **Description**:
  - 完整重写 AIEngine.h 头文件，定义完整的类接口
  - 实现 Vulkan 初始化与 GPU 检测
  - 实现 CPU 回退机制
  - 实现 ncnn::Net 封装
  - 实现 QImage ↔ ncnn::Mat 转换
- **Acceptance Criteria**: [AC-2, AC-3]
- **Test Requirements**:
  - `programmatic` TR-1.1: Vulkan 初始化成功或正确回退到 CPU
  - `programmatic` TR-1.2: GPU 信息正确记录
  - `programmatic` TR-1.3: 图像格式转换正确

## [ ] Task 2: ModelRegistry 模型注册系统
- **Priority**: P0
- **Depends On**: [Task 1]
- **Description**:
  - 创建 ModelRegistry 类
  - 实现模型文件扫描（.param/.bin 配对）
  - 定义模型元数据结构（ModelInfo）
  - 实现按类别组织模型
  - 注册已部署的所有模型
- **Acceptance Criteria**: [AC-1]
- **Test Requirements**:
  - `programmatic` TR-2.1: 所有模型文件被正确发现
  - `programmatic` TR-2.2: 模型分类正确
  - `programmatic` TR-2.3: 模型元数据完整

## [ ] Task 3: 模型加载与卸载
- **Priority**: P0
- **Depends On**: [Task 2]
- **Description**:
  - 实现 loadModel(modelId) 方法
  - 实现 unloadModel() 方法
  - 实现模型文件验证
  - 实现加载状态管理
  - 发射 modelLoaded/modelUnloaded 信号
- **Acceptance Criteria**: [AC-2, AC-9]
- **Test Requirements**:
  - `programmatic` TR-3.1: 模型加载成功返回 true
  - `programmatic` TR-3.2: 模型文件不存在返回 false
  - `programmatic` TR-3.3: 模型切换正确

## [ ] Task 4: 异步推理引擎
- **Priority**: P0
- **Depends On**: [Task 3]
- **Description**:
  - 实现 processAsync() 异步推理
  - 实现工作线程管理
  - 实现进度回调机制
  - 实现 cancelProcess() 取消机制
  - 发射 processCompleted/processError 信号
- **Acceptance Criteria**: [AC-4, AC-5, AC-6]
- **Test Requirements**:
  - `programmatic` TR-4.1: 异步推理不阻塞 UI
  - `programmatic` TR-4.2: 进度信号正确发射
  - `programmatic` TR-4.3: 取消操作正确停止推理

## [ ] Task 5: 大图分块处理
- **Priority**: P1
- **Depends On**: [Task 4]
- **Description**:
  - 实现图像分块算法
  - 实现重叠区域（overlap）处理
  - 实现分块结果合并
  - 实现分块进度计算
  - 支持可配置的分块大小
- **Acceptance Criteria**: [AC-7]
- **Test Requirements**:
  - `programmatic` TR-5.1: 大图自动分块
  - `human-judgment` TR-5.2: 合并结果无边界痕迹
  - `programmatic` TR-5.3: 内存使用在限制内

## [ ] Task 6: 模型参数系统
- **Priority**: P1
- **Depends On**: [Task 3]
- **Description**:
  - 定义模型参数结构（AIModelParams）
  - 实现 setParameter()/getParameter() 方法
  - 实现参数验证
  - 支持模型特定参数（如降噪强度、放大倍数）
- **Acceptance Criteria**: [AC-4]
- **Test Requirements**:
  - `programmatic` TR-6.1: 参数设置正确保存
  - `programmatic` TR-6.2: 无效参数被拒绝

## [ ] Task 7: ProcessingEngine 集成
- **Priority**: P0
- **Depends On**: [Task 4]
- **Description**:
  - 修改 ProcessingEngine 添加 AIEngine 实例
  - 实现处理模式路由（Shader/AI）
  - 实现 AI 任务处理流程
  - 集成进度和状态信号
  - 更新 DataTypes.h 添加 AI 相关类型
- **Acceptance Criteria**: [AC-4, AC-5]
- **Test Requirements**:
  - `programmatic` TR-7.1: AI 模式任务正确路由
  - `programmatic` TR-7.2: 进度同步正确
  - `programmatic` TR-7.3: 结果正确返回

## [ ] Task 8: AIModelPanel.qml 模型选择面板
- **Priority**: P0
- **Depends On**: [Task 7]
- **Description**:
  - 创建 AIModelPanel.qml 组件
  - 实现模型类别列表显示
  - 实现类别下模型列表显示
  - 实现模型选择交互
  - 实现模型信息展示卡片
- **Acceptance Criteria**: [AC-8]
- **Test Requirements**:
  - `human-judgment` TR-8.1: 界面布局美观
  - `programmatic` TR-8.2: 模型选择正确响应
  - `human-judgment` TR-8.3: 模型信息显示完整

## [ ] Task 9: AIParamsPanel.qml 参数配置面板
- **Priority**: P1
- **Depends On**: [Task 8]
- **Description**:
  - 创建 AIParamsPanel.qml 组件
  - 实现动态参数控件生成
  - 实现参数滑块、下拉框等控件
  - 实现参数实时预览（如适用）
  - 实现参数重置功能
- **Acceptance Criteria**: [AC-8]
- **Test Requirements**:
  - `programmatic` TR-9.1: 参数控件正确生成
  - `programmatic` TR-9.2: 参数值正确传递
  - `human-judgment` TR-9.3: 界面交互流畅

## [ ] Task 10: ControlPanel.qml AI 模式重构
- **Priority**: P0
- **Depends On**: [Task 8, Task 9]
- **Description**:
  - 重构 ControlPanel.qml 的 AI 模式部分
  - 集成 AIModelPanel 和 AIParamsPanel
  - 实现模式切换动画
  - 实现发送处理逻辑
  - 更新 sendToProcessing() 方法支持 AI 模式
- **Acceptance Criteria**: [AC-8]
- **Test Requirements**:
  - `human-judgment` TR-10.1: AI 模式界面完整
  - `programmatic` TR-10.2: 模式切换正常
  - `programmatic` TR-10.3: 发送处理正确

## [ ] Task 11: QML 数据绑定与控制器
- **Priority**: P0
- **Depends On**: [Task 7]
- **Description**:
  - 创建 AIController 或扩展现有控制器
  - 实现 Q_PROPERTY 暴露模型列表
  - 实现模型选择 Q_INVOKABLE 方法
  - 实现推理状态 Q_PROPERTY
  - 注册到 QML 引擎
- **Acceptance Criteria**: [AC-8]
- **Test Requirements**:
  - `programmatic` TR-11.1: QML 可访问模型列表
  - `programmatic` TR-11.2: 模型选择方法正确调用
  - `programmatic` TR-11.3: 状态属性正确绑定

## [ ] Task 12: 错误处理与用户反馈
- **Priority**: P1
- **Depends On**: [Task 10]
- **Description**:
  - 实现推理错误捕获
  - 实现错误信息本地化
  - 集成 Toast 提示显示错误
  - 实现加载失败降级提示
  - 实现无 GPU 时的提示
- **Acceptance Criteria**: [AC-10]
- **Test Requirements**:
  - `programmatic` TR-12.1: 错误被正确捕获
  - `human-judgment` TR-12.2: 错误信息清晰易懂
  - `programmatic` TR-12.3: 应用不崩溃

## [ ] Task 13: 国际化支持
- **Priority**: P2
- **Depends On**: [Task 10]
- **Description**:
  - 添加 AI 相关翻译字符串
  - 更新 app_zh_CN.ts 和 app_en_US.ts
  - 确保所有 UI 文本可翻译
- **Acceptance Criteria**: [AC-8]
- **Test Requirements**:
  - `human-judgment` TR-13.1: 中文界面正确
  - `human-judgment` TR-13.2: 英文界面正确

## [ ] Task 14: 集成测试与优化
- **Priority**: P2
- **Depends On**: [Task 12]
- **Description**:
  - 端到端 AI 处理流程测试
  - 性能测试（GPU/CPU 对比）
  - 内存泄漏检测
  - UI 响应测试
  - 代码质量检查
- **Acceptance Criteria**: [NFR-1, NFR-2, NFR-3, NFR-4]
- **Test Requirements**:
  - `programmatic` TR-14.1: 所有功能正常
  - `programmatic` TR-14.2: 性能达标
  - `programmatic` TR-14.3: 无内存泄漏

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 2]
- [Task 4] depends on [Task 3]
- [Task 5] depends on [Task 4]
- [Task 6] depends on [Task 3]
- [Task 7] depends on [Task 4]
- [Task 8] depends on [Task 7]
- [Task 9] depends on [Task 8]
- [Task 10] depends on [Task 8, Task 9]
- [Task 11] depends on [Task 7]
- [Task 12] depends on [Task 10]
- [Task 13] depends on [Task 10]
- [Task 14] depends on [Task 12]

# Parallelizable Tasks
以下任务可以并行执行：
- [Task 5] 和 [Task 6] 可以在 [Task 4] 完成后并行
- [Task 8] 和 [Task 11] 可以在 [Task 7] 完成后并行
- [Task 9] 和 [Task 11] 可以并行
- [Task 13] 可以在 [Task 10] 完成后与 [Task 12] 并行
